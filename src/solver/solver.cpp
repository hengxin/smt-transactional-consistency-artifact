#include "solver.h"

#include <z3++.h>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/visitors.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/to_vector.h"

using checker::history::Constraint;
using std::get;
using std::pair;
using std::reduce;
using std::unordered_map;
using std::vector;
using std::ranges::range;
using std::ranges::subrange;
using std::ranges::views::all;
using std::ranges::views::transform;
using z3::expr;

static constexpr auto hash_txn_pair = [](const pair<int64_t, int64_t> &p) {
  auto h = std::hash<int64_t>{};
  return h(p.first) ^ h(p.second);
};

static constexpr auto hash_z3_expr = [](const expr &e) { return e.hash(); };

using EdgeVarMap =
    unordered_map<pair<int64_t, int64_t>, expr, decltype(hash_txn_pair)>;
using VarEdgeMap =
    unordered_map<expr, pair<int64_t, int64_t>, decltype(hash_z3_expr)>;

static auto create_graph_from_edges(range auto edges) {
  auto graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
                                     int64_t>{};
  auto vertex_map =
      unordered_map<int64_t, decltype(graph)::vertex_descriptor>{};

  for (const auto &[from, to] : edges) {
    for (auto v : {from, to}) {
      if (!vertex_map.contains(v)) {
        vertex_map.try_emplace(v, boost::add_vertex(v, graph));
      }
    }

    boost::add_edge(vertex_map.at(from), vertex_map.at(to), graph);
  }

  return pair{graph, vertex_map};
}

namespace checker::solver {

Solver::Solver(const history::DependencyGraph &known_graph,
               const std::vector<history::Constraint> &constraints)
    : solver{context} {
  auto edge_vars = EdgeVarMap{};

  auto get_var = [&, n = 0]() mutable {
    return context.bool_const(std::to_string(n++).c_str());
  };
  auto get_edge_var = [&](int64_t from, int64_t to) mutable {
    if (auto it = edge_vars.find({from, to}); it != edge_vars.end()) {
      return it->second;
    } else {
      return edge_vars.try_emplace({from, to}, get_var()).first->second;
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
    return reduce(std::ranges::begin(r), std::ranges::end(r),
                  context.bool_val(true), std::bit_and{});
  };

  for (const auto &[from, to, _] : known_graph.edges()) {
    solver.add(get_edge_var(from, to) == context.bool_val(true));
  }

  for (const auto &c : constraints) {
    auto either_vars = c.either_edges | edge_to_var;
    auto or_vars = c.or_edges | edge_to_var;
    solver.add((and_all(negate(either_vars)) & and_all(or_vars)) |
               (and_all(either_vars) & and_all(negate(or_vars))));
  }

  user_propagator =
      std::make_unique<DependencyGraphHasNoCycle>(context, solver, edge_vars);
}

auto Solver::solve() -> bool { return solver.check() == z3::sat; }

Solver::~Solver() = default;

template <typename Graph>
struct CycleDetector : boost::dfs_visitor<> {
  using Vertex = typename Graph::vertex_descriptor;
  using Edge = typename Graph::edge_descriptor;

  vector<pair<int64_t, int64_t>> *cycle;
  unordered_map<Vertex, Vertex> predecessor;

  auto back_edge(const Edge &e, const Graph &g) -> void {
    if (!cycle->empty()) {
      return;
    }

    auto top = boost::target(e, g);
    auto bottom = boost::source(e, g);

    cycle->push_back({g[bottom], g[top]});
    for (auto v = bottom; v != top; v = predecessor.at(v)) {
      cycle->push_back({g[predecessor.at(v)], g[v]});
    }
  }

  auto tree_edge(const Edge &e, const Graph &g) -> void {
    auto from = boost::source(e, g);
    auto to = boost::target(e, g);

    predecessor[to] = from;
  }
};

struct DependencyGraphHasNoCycle : z3::user_propagator_base {
  EdgeVarMap var_map;
  VarEdgeMap edge_map;
  vector<size_t> fixed_vars_num_at_scope{0};
  vector<pair<int64_t, int64_t>> fixed_edges;
  z3::context *context;

  DependencyGraphHasNoCycle(z3::context &context, z3::solver &solver,
                            const EdgeVarMap &var_map)
      : z3::user_propagator_base(&solver), var_map{var_map}, context(&context) {
    for (const auto &[p, e] : var_map) {
      edge_map.try_emplace(e, p);
      add(e);
    }

    register_fixed();
    register_final();
  }

  auto push() -> void override {
    fixed_vars_num_at_scope.emplace_back(fixed_edges.size());
  }

  auto pop(unsigned int num_scopes) -> void override {
    fixed_vars_num_at_scope.resize(fixed_vars_num_at_scope.size() - num_scopes);
    fixed_edges.resize(fixed_vars_num_at_scope.back());
  }

  auto fixed(const expr &var, const expr &value) -> void override {
    if (value.bool_value()) {
      fixed_edges.emplace_back(edge_map.at(var));
    }
  }

  auto fresh(z3::context &ctx) -> z3::user_propagator_base * override {
    return this;
  }

  auto final() -> void override {
    auto [graph, _] = create_graph_from_edges(all(fixed_edges));
    auto cycle = vector<pair<int64_t, int64_t>>{};
    auto cycle_detector = CycleDetector<decltype(graph)>{.cycle = &cycle};
    auto visited =
        vector<boost::default_color_type>(boost::num_vertices(graph));
    auto visited_map = boost::make_iterator_property_map(
        visited.begin(), get(boost::vertex_index, graph));

    auto [begin, end] = boost::vertices(graph);
    for (auto v : subrange(begin, end)) {
      if (!get(visited_map, v)) {
        boost::depth_first_search(graph, cycle_detector, visited_map, v);

        if (!cycle.empty()) {
          break;
        }
      }
    }

    if (!cycle.empty()) {
      auto cycle_vec = z3::expr_vector{*context};
      for (auto p : cycle) {
        cycle_vec.push_back(var_map.at(p));
      }

      conflict(cycle_vec);
    }
  }
};

}  // namespace checker::solver
