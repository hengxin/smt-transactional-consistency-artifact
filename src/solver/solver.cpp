#include "solver.h"

#include <z3++.h>

#include <algorithm>
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
using std::vector;
using std::ranges::range;
using std::ranges::subrange;
using std::ranges::views::all;
using std::ranges::views::filter;
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

namespace checker::utils {
static inline auto operator|(const z3::expr_vector v, ToUnorderedSet)
    -> std::unordered_set<expr> {
  auto s = std::unordered_set<expr>{};

  for (const auto &e : v) {
    s.emplace(e);
  }

  return s;
}
}  // namespace checker::utils

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
static auto dependency_graph_of(const Graph &polygraph, range auto edges) {
  using Edge = typename Graph::edge_descriptor;
  using HashSet = std::unordered_set<Edge, boost::hash<Edge>>;

  auto filter_edges =
      [edgeset = std::make_shared<HashSet>(edges | to<HashSet>)](Edge e) {
        return edgeset->contains(e);
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
    : solver{context, z3::solver::simple{}}, known_vars{context} {
  CHECKER_LOG_COND(trace, logger) {
    logger << "wr:\n" << known_graph.wr << "cons:\n";
    for (const auto &c : constraints) {
      logger << c;
    }
  }

  auto polygraph = utils::Graph<int64_t, expr>{};

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

  for (const auto &[from, to, _] : known_graph.edges()) {
    auto known_var = get_edge_var(from, to);
    BOOST_LOG_TRIVIAL(trace) << "known: " << known_var.to_string();
    known_vars.push_back(known_var);
  }

  auto ww_vars = z3::expr_vector{context};
  for (const auto &c : constraints) {
    auto either_vars = c.either_edges | edge_to_var | to_vector;
    auto or_vars = c.or_edges | edge_to_var | to_vector;
    auto cons_var =
        (either_vars[0] && !or_vars[0]) || (!either_vars[0] && or_vars[0]);

    BOOST_LOG_TRIVIAL(trace) << "constraint: " << cons_var.to_string();
    solver.add(cons_var);
    ww_vars.push_back(either_vars[0]);
    ww_vars.push_back(or_vars[0]);
  }

  user_propagator = std::make_unique<DependencyGraphHasNoCycle>(
      solver, std::move(polygraph), known_vars, ww_vars, constraints);
}

auto Solver::solve() -> bool { return solver.check(known_vars) == z3::sat; }

Solver::~Solver() = default;

struct DependencyGraphHasNoCycle : z3::user_propagator_base {
  using Graph = utils::Graph<int64_t, expr>;
  using Vertex = Graph::InternalGraph::vertex_descriptor;
  using Edge = Graph::InternalGraph::edge_descriptor;
  using DependencyGraph =
      decltype(dependency_graph_of(Graph::InternalGraph{}, vector<Edge>{}));

  Graph polygraph;

  vector<size_t> fixed_edges_num;
  vector<size_t> fixed_vars_num;
  vector<Edge> fixed_edges;
  z3::expr_vector fixed_vars;
  std::unordered_set<expr> fixed_vars_set;

  vector<Vertex> topo_order;

  unordered_map<Edge, vector<Edge>, boost::hash<Edge>> rw_map;
  unordered_map<Edge, vector<expr>, boost::hash<Edge>> ww_var_map;

  DependencyGraphHasNoCycle(z3::solver &solver, Graph &&_polygraph,
                            const z3::expr_vector &known_vars,
                            const z3::expr_vector &ww_vars,
                            const vector<Constraint> &constraints)
      : z3::user_propagator_base{&solver},
        polygraph{std::move(_polygraph)},
        fixed_vars{ctx()} {
    for (const auto &known_var : known_vars) {
      add(known_var);
    }
    for (const auto &ww_var : ww_vars) {
      add(ww_var);
    }

    for (auto v : polygraph.vertices()) {
      topo_order.push_back(polygraph.vertex_map.left.at(v));
    }

    for (const auto &c : constraints) {
      auto fill_rw_map = [&](const vector<Constraint::Edge> &e) {
        auto get_edge = [&](const Constraint::Edge &e) {
          const auto &[from, to, _] = e;
          auto [desc, has_edge] =
              boost::edge(polygraph.vertex_map.left.at(from),
                          polygraph.vertex_map.left.at(to), *polygraph.graph);

          assert(has_edge);
          return desc;
        };

        auto ww_edge = get_edge(e[0]);
        auto rw_edges =
            subrange(e.begin() + 1, e.end()) | transform(get_edge) | to_vector;
        std::ranges::copy(rw_edges, std::back_inserter(rw_map[ww_edge]));

        auto [from, to, _] = e[0];
        auto ww_var = polygraph.edge(from, to).value().get();
        for (const auto &rw_edge : rw_edges) {
          ww_var_map[rw_edge].emplace_back(ww_var);
        }
      };

      fill_rw_map(c.either_edges);
      fill_rw_map(c.or_edges);
    }

    register_fixed();
  }

  auto push() -> void override {
    BOOST_LOG_TRIVIAL(trace) << "push";
    fixed_edges_num.emplace_back(fixed_edges.size());
    fixed_vars_num.emplace_back(fixed_vars.size());
  }

  auto pop(unsigned int num_scopes) -> void override {
    BOOST_LOG_TRIVIAL(trace) << "pop " << num_scopes;

    auto remaining_edges_num =
        fixed_edges_num.at(fixed_edges_num.size() - num_scopes);
    auto remaining_vars_num =
        fixed_vars_num.at(fixed_vars_num.size() - num_scopes);
    fixed_edges_num.resize(fixed_edges_num.size() - num_scopes);
    fixed_vars_num.resize(fixed_vars_num.size() - num_scopes);

    CHECKER_LOG_COND(trace, logger) {
      logger << "unfix:";
      for (auto i = remaining_vars_num; i < fixed_vars.size(); i++) {
        logger << ' ' << fixed_vars[i].to_string();
      }
    }

    for (auto i = remaining_vars_num; i < fixed_vars.size(); i++) {
      fixed_vars_set.erase(fixed_vars[i]);
    }
    fixed_edges.erase(fixed_edges.begin() + remaining_edges_num,
                      fixed_edges.end());
    fixed_vars.resize(remaining_vars_num);
  }

  auto fixed(const expr &ww_var, const expr &value) -> void override {
    if (value.bool_value() != Z3_L_TRUE) {
      return;
    }
    BOOST_LOG_TRIVIAL(trace) << "fixed: " << ww_var.to_string();

    auto ww_edge = polygraph.edge_map.left.at(ww_var);
    fixed_edges.emplace_back(ww_edge);
    fixed_vars.push_back(ww_var);
    fixed_vars_set.emplace(ww_var);

    auto graph = dependency_graph_of(*polygraph.graph, fixed_edges);

    propagate_rw_edge(graph, ww_edge);
    if (!detect_cycle(graph, ww_edge)) {
      return;
    }

    // TODO(czg): adapt to propagate_rw_edge
    // propagate_unit_edge(graph, ww_edge);
  }

  auto fresh(z3::context &ctx) -> z3::user_propagator_base * override {
    return this;
  }

  auto detect_cycle(DependencyGraph dependency_graph, Edge added_edge) -> bool {
    if (checker::utils::toposort_add_edge(dependency_graph, topo_order,
                                          added_edge)) {
      return true;
    }

    auto cycle = vector<DependencyGraph::edge_descriptor>{};
    auto cycle_detector =
        CycleDetector<DependencyGraph>{.cycle = std::ref(cycle)};
    depth_first_visit(dependency_graph,
                      boost::source(added_edge, dependency_graph),
                      cycle_detector);
    assert(!cycle.empty());

    auto cycle_vars_set = std::unordered_set<expr>{};
    CHECKER_LOG_COND(trace, logger) {
      logger << "conflict:";
      for (auto e : cycle) {
        logger << ' ' << polygraph.edge_map.right.at(e).to_string();
      }
    }
    for (auto e : cycle) {
      auto ww_vars = ww_var_map[e] | filter([&](const expr &e) {
                       return fixed_vars_set.contains(e);
                     });
      for (const auto &ww_var : ww_vars) {
        cycle_vars_set.emplace(ww_var);
      }
    }

    auto cycle_vars = z3::expr_vector{ctx()};
    for (const auto &cycle_var : cycle_vars_set) {
      cycle_vars.push_back(cycle_var);
    }
    conflict(cycle_vars);
    return false;
  }

  auto propagate_unit_edge(DependencyGraph dependency_graph, Edge added_edge)
      -> void {
    auto from = boost::source(added_edge, dependency_graph);
    auto to = boost::target(added_edge, dependency_graph);

    auto to_successors = transitive_successors_of(dependency_graph, to);
    auto from_predecessors = transitive_successors_of(
        boost::make_reverse_graph(dependency_graph), from);
    auto unit_edge_vars = vector<expr>{};

    for (const auto &s : to_successors) {
      for (const auto &p : from_predecessors) {
        auto [edge, has_edge] = boost::edge(s, p, *polygraph.graph);
        if (has_edge) {
          unit_edge_vars.push_back(polygraph.edge_map.right.at(edge));
        }
      }
    }

    if (!unit_edge_vars.empty()) {
      auto negation =
          unit_edge_vars | transform([](const expr &e) { return !e; });
      auto consequence = reduce(negation.begin(), negation.end(),
                                ctx().bool_val(true), std::bit_and<>{});

      BOOST_LOG_TRIVIAL(trace) << "propagate: " << fixed_vars.to_string()
                               << " => " << consequence.to_string();
      propagate(fixed_vars, consequence);
    }
  }

  auto propagate_rw_edge(DependencyGraph dependency_graph, Edge added_edge)
      -> void {
    if (rw_map.contains(added_edge)) {
      std::ranges::copy(rw_map.at(added_edge), std::back_inserter(fixed_edges));
    }
  }
};

}  // namespace checker::solver
