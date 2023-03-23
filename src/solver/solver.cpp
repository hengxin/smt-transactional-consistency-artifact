#include "solver.h"

#include <z3++.h>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/log/trivial.hpp>
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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/graph.h"
#include "utils/to_vector.h"
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
using std::vector;
using std::ranges::range;
using std::ranges::subrange;
using std::ranges::views::all;
using std::ranges::views::iota;
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

using Graph = checker::utils::Graph<int64_t, expr>;

template <typename Graph>
static auto dependency_graph_of(const Graph &polygraph, range auto edges) {
  using Edge = typename Graph::edge_descriptor;
  using HashSet = std::unordered_set<Edge, boost::hash<Edge>>;

  auto filter_edges =
      [edgeset = std::make_shared<HashSet>(edges | to<HashSet>)](
          Edge e) { return edgeset->contains(e); };

  return boost::filtered_graph{
    polygraph,
    std::function{std::move(filter_edges)},
  };
}

namespace checker::solver {

Solver::Solver(const history::DependencyGraph &known_graph,
               const vector<history::Constraint> &constraints)
    : solver{context, z3::solver::simple{}} {
  BOOST_LOG_TRIVIAL(trace) << "wr:\n" << known_graph.wr << "\ncons:\n";
  for (const auto &c : constraints) {
    BOOST_LOG_TRIVIAL(trace) << c << "\n";
  }

  auto polygraph = Graph{};
  auto bool_true = context.bool_val(true);

  auto get_vertex = [&](int64_t id) {
    if (auto v = polygraph.vertex(id); v) {
      return v.value().get();
    } else {
      return polygraph.add_vertex(id);
    }
  };
  auto get_edge_var = [&](int64_t from, int64_t to) {
    auto from_desc = get_vertex(from);
    auto to_desc = get_vertex(to);

    if (auto e = polygraph.edge(from, to); e) {
      return e.value().get();
    } else {
      std::stringstream name;
      name << from << "->" << to;

      auto edge_var = context.bool_const(name.str().c_str());
      return polygraph.add_edge(from_desc, to_desc, edge_var);
    }
  };
  auto edge_to_var = transform([&](const Constraint::Edge &e) {
    const auto &[from, to, _] = e;
    return get_edge_var(from, to);
  });
  auto negate = [&](range auto r) {
    return r | transform([](const expr &e) { return !e; });
  };
  auto and_all = [&](range auto r) {
    return reduce(std::ranges::begin(r), std::ranges::end(r), bool_true,
                  std::bit_and{});
  };

  for (const auto &[from, to, _] : known_graph.edges()) {
    auto known_var = get_edge_var(from, to);
    BOOST_LOG_TRIVIAL(trace) << "known: " << known_var.to_string() << '\n';

    solver.add(known_var == bool_true);
  }

  for (const auto &c : constraints) {
    auto either_vars = c.either_edges | edge_to_var;
    auto or_vars = c.or_edges | edge_to_var;
    auto cons_var = (and_all(negate(either_vars)) & and_all(or_vars)) |
                    (and_all(either_vars) & and_all(negate(or_vars)));

    BOOST_LOG_TRIVIAL(trace) << "constraint: " << cons_var.to_string() << '\n';
    solver.add(cons_var);
  }

  user_propagator =
      std::make_unique<DependencyGraphHasNoCycle>(solver, std::move(polygraph));
}

auto Solver::solve() -> bool { return solver.check() == z3::sat; }

Solver::~Solver() = default;

template <typename Graph>
struct CycleDetector : boost::dfs_visitor<> {
  using Vertex = typename Graph::vertex_descriptor;
  using Edge = typename Graph::edge_descriptor;

  std::reference_wrapper<vector<Edge>> cycle;
  unordered_map<Vertex, Vertex> pred_edge;

  auto back_edge(const Edge &e, const Graph &g) -> void {
    if (!cycle.get().empty()) {
      return;
    }

    auto top = boost::target(e, g);
    auto bottom = boost::source(e, g);

    cycle.get().push_back(e);
    for (auto v = bottom; v != top; v = pred_edge.at(v)) {
      cycle.get().push_back(boost::edge(pred_edge[v], v, g).first);
    }
  }

  auto tree_edge(const Edge &e, const Graph &g) -> void {
    auto from = boost::source(e, g);
    auto to = boost::target(e, g);

    pred_edge[to] = from;
  }
};

struct DependencyGraphHasNoCycle : z3::user_propagator_base {
  Graph polygraph;

  vector<size_t> fixed_edges_num;
  vector<adjacency_list<>::edge_descriptor> fixed_edges;

  vector<adjacency_list<>::vertex_descriptor> topo_order;

  DependencyGraphHasNoCycle(z3::solver &solver, Graph &&_polygraph)
      : z3::user_propagator_base{&solver}, polygraph{std::move(_polygraph)} {
    for (auto [_, __, edge] : polygraph.edges()) {
      add(edge.get());
    }

    for (auto v : polygraph.vertices()) {
      topo_order.push_back(polygraph.vertex_map.left.at(v));
    }

    register_fixed();
  }

  auto push() -> void override {
    BOOST_LOG_TRIVIAL(trace) << "push\n";
    fixed_edges_num.emplace_back(fixed_edges.size());
  }

  auto pop(unsigned int num_scopes) -> void override {
    BOOST_LOG_TRIVIAL(trace) << "pop " << num_scopes << "\n";

    auto remains_num = fixed_edges_num[fixed_edges_num.size() - num_scopes];
    fixed_edges_num.resize(fixed_edges_num.size() - num_scopes);

    BOOST_LOG_TRIVIAL(trace) << "unfix: ";
    for (auto i = remains_num; i < fixed_edges.size(); i++) {
      auto e = fixed_edges[i];
      BOOST_LOG_TRIVIAL(trace) << polygraph.edge_map.right.at(e).to_string();
    }
    BOOST_LOG_TRIVIAL(trace) << "\n";

    fixed_edges.erase(fixed_edges.begin() + remains_num, fixed_edges.end());
  }

  auto fixed(const expr &var, const expr &value) -> void override {
    if (value.bool_value() != Z3_L_TRUE) {
      return;
    }

    BOOST_LOG_TRIVIAL(trace) << "fixed: " << var.to_string() << "\n";

    auto edge_desc = polygraph.edge_map.left.at(var);
    fixed_edges.emplace_back(edge_desc);

    auto graph = dependency_graph_of(*polygraph.graph, fixed_edges);
    if (checker::utils::toposort_add_edge(graph, topo_order, edge_desc)) {
      return;
    }

    auto cycle = vector<decltype(graph)::edge_descriptor>{};
    auto cycle_detector =
        CycleDetector<decltype(graph)>{.cycle = std::ref(cycle)};
    boost::depth_first_search(graph, boost::visitor(cycle_detector).root_vertex(boost::source(edge_desc, graph)));
    assert(!cycle.empty());

    auto cycle_vars = z3::expr_vector{ctx()};
    BOOST_LOG_TRIVIAL(trace) << "conflict: ";
    for (auto e : cycle) {
      auto var = polygraph.edge_map.right.at(e);
      BOOST_LOG_TRIVIAL(trace) << var.to_string();
      cycle_vars.push_back(var);
    }
    BOOST_LOG_TRIVIAL(trace) << '\n';

    conflict(cycle_vars);
  }

  auto fresh(z3::context &ctx) -> z3::user_propagator_base * override {
    return this;
  }
};

}  // namespace checker::solver
