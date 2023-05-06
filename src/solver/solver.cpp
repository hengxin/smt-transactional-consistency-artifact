#include "solver.h"

#include <z3++.h>

#include <algorithm>
#include <boost/container_hash/extensions.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/reverse_graph.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/log/trivial.hpp>
#include <boost/property_map/vector_property_map.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/to_container.h"
#include "utils/toposort.h"

using boost::add_edge;
using boost::add_vertex;
using boost::adjacency_list;
using boost::bidirectionalS;
using boost::directedS;
using boost::edge;
using boost::no_property;
using boost::num_vertices;
using boost::remove_edge;
using boost::source;
using boost::target;
using boost::vecS;
using boost::vertex_index_t;
using checker::history::Constraint;
using checker::utils::to;
using checker::utils::to_unordered_map;
using checker::utils::to_unordered_set;
using checker::utils::to_vector;
using std::get;
using std::optional;
using std::pair;
using std::reduce;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::ranges::copy;
using std::ranges::range;
using std::ranges::subrange;
using std::ranges::views::all;
using std::ranges::views::drop;
using std::ranges::views::filter;
using std::ranges::views::iota;
using std::ranges::views::keys;
using std::ranges::views::reverse;
using std::ranges::views::transform;
using z3::expr;

template <>
struct std::hash<expr> {
  auto operator()(const expr &e) const -> size_t { return e.hash(); }
};

template <>
struct std::equal_to<expr> {
  auto operator()(const expr &left, const expr &right) const -> bool {
    return left.id() == right.id();
  }
};

template <typename Graph>
static auto vertices(const Graph &graph) {
  auto [begin, end] = boost::vertices(graph);
  return subrange(begin, end);
}

namespace checker::solver {

Solver::Solver(const history::DependencyGraph &known_graph,
               const vector<history::Constraint> &constraints)
    : solver{context, z3::solver::simple{}} {
  using Graph = adjacency_list<vecS, vecS, directedS, int64_t>;
  using Vertex = Graph::vertex_descriptor;
  using Edge = Graph::edge_descriptor;
  using EdgeSet = unordered_set<Edge, boost::hash<Edge>>;

  CHECKER_LOG_COND(trace, logger) {
    logger << "wr:\n" << known_graph.wr << "cons:\n";
    for (const auto &c : constraints) {
      logger << c;
    }
  }

  auto polygraph = Graph{};
  auto vertex_map = unordered_map<int64_t, Vertex>{};

  auto get_vertex = [&](int64_t id) {
    if (vertex_map.contains(id)) {
      return vertex_map.at(id);
    }
    return vertex_map.try_emplace(id, add_vertex(id, polygraph)).first->second;
  };
  auto get_edge = [&](int64_t from, int64_t to) {
    auto from_desc = get_vertex(from);
    auto to_desc = get_vertex(to);

    return add_edge(from_desc, to_desc, polygraph).first;
  };
  auto add_constraint_edge = transform([&](const Constraint::Edge &e) {
    const auto &[from, to, _] = e;
    return get_edge(from, to);
  });

  auto constraint_edges = unordered_map<expr, EdgeSet>{};

  // use true as a dummy constraint variable for known edges
  auto z3_true = context.bool_val(true);
  auto known_edges = EdgeSet{};
  for (const auto &[from, to, _] : known_graph.edges()) {
    BOOST_LOG_TRIVIAL(trace) << "known: " << from << "->" << to;
    known_edges.emplace(get_edge(from, to));
  }
  constraint_edges.try_emplace(z3_true, std::move(known_edges));

  for (const auto &c : constraints) {
    std::stringstream either_name, or_name;
    either_name << c.either_txn_id << "->" << c.or_txn_id;
    or_name << c.or_txn_id << "->" << c.either_txn_id;

    auto either_var = context.bool_const(either_name.str().c_str());
    auto or_var = context.bool_const(or_name.str().c_str());
    solver.add(either_var ^ or_var);

    constraint_edges.try_emplace(
        either_var, c.either_edges | add_constraint_edge | to<EdgeSet>);
    constraint_edges.try_emplace(
        or_var, c.or_edges | add_constraint_edge | to<EdgeSet>);
  }

  user_propagator = std::make_unique<DependencyGraphHasNoCycle>(
      solver, std::move(polygraph), std::move(constraint_edges));
}

auto Solver::solve() -> bool { return solver.check() == z3::sat; }

Solver::~Solver() = default;

struct DependencyGraphHasNoCycle : z3::user_propagator_base {
  using PolyGraph = adjacency_list<vecS, vecS, directedS, int64_t>;
  using PolyGraphVertex = PolyGraph::vertex_descriptor;
  using PolyGraphEdge = PolyGraph::edge_descriptor;
  using PolyGraphEdgeSet =
      unordered_set<PolyGraphEdge, boost::hash<PolyGraphEdge>>;

  using Graph =
      adjacency_list<vecS, vecS, bidirectionalS, no_property, vector<expr>>;
  using Vertex = Graph::vertex_descriptor;
  using Edge = Graph::edge_descriptor;

  static_assert(std::is_same_v<PolyGraphVertex, Vertex> &&
                std::is_integral_v<Vertex>);

  PolyGraph polygraph;

  /*
   * A dependency graph contains the current set of fixed edges of the
   * polygraph. Each edge has a set of variables that enables it. These
   * variables are currently assigned true by Z3.
   */
  Graph dependency_graph;
  utils::IncrementalCycleDetector<Graph> cycle_detector;

  /*
   * Z3 uses a stack of fixed variables internally. When push() is called, a new
   * scope is pushed to the stack. When pop(lvl) is called, $lvl scopes are
   * poped from the stack. Each scope contains one or more fixed variables.
   * fixed(var, val) is called when a variable is assigned a value.
   *
   * We maintain a stack that mirrors the structure of Z3's using the variables
   * fixed_vars_num and fixed_vars.
   */
  vector<size_t> fixed_vars_num;   // total number of fixed variables at this
                                   // scope and lower scope
  vector<expr> fixed_vars;         // fixed variables at each scope
  vector<size_t> fixed_edges_num;  // total number of fixed edges
  vector<Edge> fixed_edges;        // stack of fixed edges

  unordered_map<expr, PolyGraphEdgeSet> constraint_to_edge_map;

  size_t n_fixed_called = 0;
  size_t n_conflicts_returned = 0;
  size_t n_conflict_vars_returned = 0;
  size_t n_pop_called = 0;
  size_t n_pop_scopes = 0;

  DependencyGraphHasNoCycle(
      z3::solver &solver, PolyGraph &&polygraph,
      unordered_map<expr, PolyGraphEdgeSet> &&constraint_edges)
      : z3::user_propagator_base{&solver},
        polygraph{std::move(polygraph)},
        dependency_graph{num_vertices(this->polygraph)},
        cycle_detector{dependency_graph},
        constraint_to_edge_map{std::move(constraint_edges)} {
    for (const auto &var : keys(constraint_to_edge_map)) {
      BOOST_LOG_TRIVIAL(trace) << "add: " << var.to_string();
      add(var);
    }

    register_fixed();
  }

  auto push() -> void override {
    BOOST_LOG_TRIVIAL(trace) << "push";
    fixed_vars_num.emplace_back(fixed_vars.size());
    fixed_edges_num.emplace_back(fixed_edges.size());
  }

  auto pop(unsigned int num_scopes) -> void override {
    n_pop_called++;
    n_pop_scopes += num_scopes;
    BOOST_LOG_TRIVIAL(trace) << "pop " << num_scopes;

    auto remaining_scopes = fixed_vars_num.size() - num_scopes;
    auto remaining_vars_num = fixed_vars_num.at(remaining_scopes);
    auto remaining_edges_num = fixed_edges_num.at(remaining_scopes);
    fixed_vars_num.resize(remaining_scopes);
    fixed_edges_num.resize(remaining_scopes);

    CHECKER_LOG_COND(trace, logger) {
      logger << "unfix:";
      for (auto &&var : fixed_vars | drop(remaining_vars_num)) {
        logger << ' ' << var.to_string();
      }
    }

    for (auto &&e : fixed_edges | drop(remaining_edges_num)) {
      dependency_graph[e].pop_back();
      if (dependency_graph[e].empty()) {
        boost::remove_edge(e, dependency_graph);
      }
    }

    fixed_vars.erase(fixed_vars.begin() + remaining_vars_num, fixed_vars.end());
    fixed_edges.erase(fixed_edges.begin() + remaining_edges_num,
                      fixed_edges.end());
  }

  auto fixed(const expr &var, const expr &value) -> void override {
    if (value.is_false()) {
      return;
    }

    BOOST_LOG_TRIVIAL(trace) << "fixed: " << var.to_string();
    n_fixed_called++;
    fixed_vars.push_back(var);

    for (const auto &pe : constraint_to_edge_map.at(var)) {
      auto e = add_edge(source(pe, polygraph), target(pe, polygraph),
                        dependency_graph)
                   .first;
      fixed_edges.emplace_back(e);
      dependency_graph[e].emplace_back(var);

      if (!detect_cycle(e)) {
        return;
      }
    }
  }

  auto fresh(z3::context &ctx) -> z3::user_propagator_base * override {
    return this;
  }

  auto detect_cycle(Edge added_edge) -> bool {
    auto cycle = cycle_detector.check_add_edge(added_edge);
    if (!cycle) {
      return true;
    }

    CHECKER_LOG_COND(trace, logger) {
      logger << "conflict:";
      for (auto e : cycle.value()) {
        logger << ' ' << polygraph[source(e, dependency_graph)] << "->"
               << polygraph[target(e, dependency_graph)];
      }
    }

    auto cycle_exprs = z3::expr_vector{ctx()};
    for (auto e : cycle.value()) {
      cycle_exprs.push_back(dependency_graph[e].back());
    }

    CHECKER_LOG_COND(trace, logger) {
      logger << "conflict expr:";
      for (const auto &e : cycle_exprs) {
        logger << ' ' << e.to_string();
      }
    }

    n_conflicts_returned++;
    n_conflict_vars_returned += cycle_exprs.size();
    conflict(cycle_exprs);
    return false;
  }

  ~DependencyGraphHasNoCycle() override {
    BOOST_LOG_TRIVIAL(debug) << "fixed() called " << n_fixed_called << " times";
    BOOST_LOG_TRIVIAL(debug)
        << "found conflict " << n_conflicts_returned << " times";
    BOOST_LOG_TRIVIAL(debug)
        << "avg. conflict length: "
        << static_cast<float>(n_conflict_vars_returned) / n_conflicts_returned;
    BOOST_LOG_TRIVIAL(debug) << "pop() called " << n_pop_called << " times";
    BOOST_LOG_TRIVIAL(debug) << "avg. pop scopes: "
                             << static_cast<float>(n_pop_scopes) / n_pop_called;
  }
};

}  // namespace checker::solver
