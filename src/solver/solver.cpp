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
#include "utils/graph.h"
#include "utils/log.h"
#include "utils/to_container.h"
#include "utils/toposort.h"

using boost::adjacency_list;
using boost::directedS;
using boost::vecS;
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
struct TransitiveSuccessorsRecorder : boost::dfs_visitor<> {
  using Vertex = typename Graph::vertex_descriptor;

  Vertex root_vertex;
  std::reference_wrapper<vector<Vertex>> successors;

  auto discover_vertex(const Vertex &v, const Graph &g) {
    successors.get().emplace_back(v);
  }
};

template <typename Graph>
static auto dependency_graph_of(const Graph &polygraph, const auto &edges) {
  auto filter_edges = [&](typename Graph::edge_descriptor e) {
    return edges.contains(e);
  };

  return boost::filtered_graph{
      polygraph,
      std::function{std::move(filter_edges)},
  };
}

static auto depth_first_visit(const auto &graph, auto root_vertex, auto visitor)
    -> void {
  auto visited = vector<boost::default_color_type>{boost::num_vertices(graph)};
  auto visited_map = boost::make_iterator_property_map(
      visited.begin(), boost::get(boost::vertex_index, graph));

  boost::depth_first_visit(graph, root_vertex, visitor, visited_map);
}

template <typename Graph>
static auto transitive_successors_of(
    const Graph &graph, typename Graph::vertex_descriptor root_vertex) {
  auto successors = vector<typename Graph::vertex_descriptor>{};
  auto recorder = TransitiveSuccessorsRecorder<Graph>{
      .root_vertex = root_vertex,
      .successors = std::ref(successors),
  };

  depth_first_visit(graph, root_vertex, recorder);
  return successors;
}

namespace checker::solver {

Solver::Solver(const history::DependencyGraph &known_graph,
               const vector<history::Constraint> &constraints)
    : solver{context, z3::solver::simple{}} {
  using Graph = utils::Graph<int64_t, std::tuple<>>;
  using Edge = Graph::InternalGraph::edge_descriptor;
  using EdgeSet = unordered_set<Edge, boost::hash<Edge>>;

  CHECKER_LOG_COND(trace, logger) {
    logger << "wr:\n" << known_graph.wr << "cons:\n";
    for (const auto &c : constraints) {
      logger << c;
    }
  }

  auto polygraph = Graph{};

  auto get_vertex = [&](int64_t id) {
    if (auto v = polygraph.vertex(id); v) {
      return polygraph.vertex_map.left.at(v.value().get());
    } else {
      return polygraph.vertex_map.left.at(polygraph.add_vertex(id));
    }
  };
  auto get_edge_var = [&](int64_t from, int64_t to) {
    auto from_desc = get_vertex(from);
    auto to_desc = get_vertex(to);

    polygraph.add_edge(from, to, {});
    return boost::edge(from_desc, to_desc, *polygraph.graph).first;
  };
  auto edge_to_var = transform([&](const Constraint::Edge &e) {
    const auto &[from, to, _] = e;
    return get_edge_var(from, to);
  });

  auto constraint_edges = unordered_map<expr, EdgeSet>{};

  // use true as a dummy constraint variable for known edges
  auto z3_true = context.bool_val(true);
  auto known_edges = EdgeSet{};
  for (const auto &[from, to, _] : known_graph.edges()) {
    auto known_var = get_edge_var(from, to);
    BOOST_LOG_TRIVIAL(trace) << "known: " << from << "->" << to;
    known_edges.emplace(known_var);
  }
  constraint_edges.try_emplace(z3_true, std::move(known_edges));

  for (const auto &c : constraints) {
    std::stringstream either_name, or_name;
    either_name << c.either_txn_id << "->" << c.or_txn_id;
    or_name << c.or_txn_id << "->" << c.either_txn_id;

    auto either_var = context.bool_const(either_name.str().c_str());
    auto or_var = context.bool_const(or_name.str().c_str());
    solver.add(either_var ^ or_var);

    constraint_edges.try_emplace(either_var,
                                 c.either_edges | edge_to_var | to<EdgeSet>);
    constraint_edges.try_emplace(or_var,
                                 c.or_edges | edge_to_var | to<EdgeSet>);
  }

  user_propagator = std::make_unique<DependencyGraphHasNoCycle>(
      solver, std::move(polygraph), std::move(constraint_edges));
}

auto Solver::solve() -> bool { return solver.check() == z3::sat; }

Solver::~Solver() = default;

struct DependencyGraphHasNoCycle : z3::user_propagator_base {
  using Graph = utils::Graph<int64_t, std::tuple<>>;
  using Vertex = Graph::InternalGraph::vertex_descriptor;
  using Edge = Graph::InternalGraph::edge_descriptor;
  using EdgeSet = unordered_set<Edge, boost::hash<Edge>>;
  using DependencyGraph =
      decltype(dependency_graph_of(Graph::InternalGraph{}, EdgeSet{}));

  Graph polygraph;

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
  z3::expr_vector fixed_vars;      // fixed variables at each scope
  vector<size_t> fixed_edges_num;  // total number of fixed edges
  vector<Edge> fixed_edges;        // stack of fixed edges

  // Map from each fixed edges to the corresponding variables.
  unordered_map<Edge, vector<expr>, boost::hash<Edge>> edge_to_expr_map;

  unordered_map<expr, EdgeSet> constraint_to_edge_map;
  utils::TopologicalOrder<Vertex> topo_order;

  DependencyGraphHasNoCycle(z3::solver &solver, Graph &&_polygraph,
                            unordered_map<expr, EdgeSet> &&constraint_edges)
      : z3::user_propagator_base{&solver},
        polygraph{std::move(_polygraph)},
        fixed_vars{ctx()},
        constraint_to_edge_map{std::move(constraint_edges)},
        topo_order{polygraph.vertices()} {
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
    BOOST_LOG_TRIVIAL(trace) << "pop " << num_scopes;

    auto remaining_scopes = fixed_vars_num.size() - num_scopes;
    auto remaining_vars_num = fixed_vars_num.at(remaining_scopes);
    auto remaining_edges_num = fixed_edges_num.at(remaining_scopes);
    fixed_vars_num.resize(remaining_scopes);
    fixed_edges_num.resize(remaining_scopes);

    CHECKER_LOG_COND(trace, logger) {
      logger << "unfix:";
      for (auto i = remaining_vars_num; i < fixed_vars.size(); i++) {
        logger << ' ' << fixed_vars[i].to_string();
      }
    }

    for (auto i = remaining_edges_num; i < fixed_edges.size(); i++) {
      auto it = edge_to_expr_map.find(fixed_edges.at(i));
      assert(it != edge_to_expr_map.end());

      it->second.pop_back();
      if (it->second.empty()) {
        edge_to_expr_map.erase(it);
      }
    }

    fixed_vars.resize(remaining_vars_num);
    fixed_edges.erase(fixed_edges.begin() + remaining_edges_num,
                      fixed_edges.end());
  }

  auto fixed(const expr &var, const expr &value) -> void override {
    if (value.is_false()) {
      return;
    }

    BOOST_LOG_TRIVIAL(trace) << "fixed: " << var.to_string();
    fixed_vars.push_back(var);

    const auto &added_edges = constraint_to_edge_map.at(var);
    auto graph = dependency_graph_of(*polygraph.graph, edge_to_expr_map);

    for (const auto &e : added_edges) {
      fixed_edges.emplace_back(e);
      edge_to_expr_map[e].emplace_back(var);

      if (!detect_cycle(graph, e)) {
        return;
      }
    }
  }

  auto fresh(z3::context &ctx) -> z3::user_propagator_base * override {
    return this;
  }

  auto detect_cycle(const DependencyGraph &dependency_graph, Edge added_edge)
      -> bool {
    auto cycle = checker::utils::toposort_add_edge(dependency_graph, topo_order,
                                                   added_edge);
    if (!cycle) {
      return true;
    }

    CHECKER_LOG_COND(trace, logger) {
      logger << "conflict:";
      for (auto e : cycle.value()) {
        logger << ' '
               << polygraph.vertex_map.right.at(
                      boost::source(e, dependency_graph))
               << "->"
               << polygraph.vertex_map.right.at(
                      boost::target(e, dependency_graph));
      }
    }

    auto cycle_exprs = z3::expr_vector{ctx()};
    for (auto e : cycle.value()) {
      cycle_exprs.push_back(edge_to_expr_map.at(e).back());
    }

    CHECKER_LOG_COND(trace, logger) {
      logger << "conflict expr:";
      for (const auto &e : cycle_exprs) {
        logger << ' ' << e.to_string();
      }
    }
    conflict(cycle_exprs);
    return false;
  }
};

}  // namespace checker::solver
