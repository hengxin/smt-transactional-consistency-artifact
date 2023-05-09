#include "pruner.h"

#include <boost/dynamic_bitset.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/adjacency_matrix.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/ranges.h"
#include "utils/to_container.h"

using boost::add_edge;
using boost::adjacency_list;
using boost::adjacency_matrix;
using boost::dynamic_bitset;
using boost::no_named_parameters;
using boost::not_a_dag;
using boost::out_edges;
using boost::target;
using boost::topological_sort;
using checker::history::Constraint;
using checker::history::DependencyGraph;
using checker::history::EdgeType;
using checker::utils::as_range;
using checker::utils::to;
using std::back_inserter;
using std::nullopt;
using std::optional;
using std::unordered_set;
using std::vector;
using std::ranges::copy;
using std::ranges::views::filter;

namespace checker::solver {
auto prune_constraints(DependencyGraph &dependency_graph,
                       vector<Constraint> &constraints) -> bool {
  auto &&vertex_map = dependency_graph.rw.vertex_map.left;
  auto pruned_constraints = unordered_set<Constraint *>{};
  auto not_pruned =
      filter([&](auto &&c) { return !pruned_constraints.contains(&c); });
  bool changed = true;

  CHECKER_LOG_COND(trace, logger) {
    logger << "vertex_map:";
    for (auto &&[v, desc] : vertex_map) {
      logger << " (" << v << ", " << desc << ')';
    }
  }

  while (changed) {
    changed = false;

    auto known_graph = adjacency_list<>{dependency_graph.num_vertices()};
    for (auto &&[from, to, _] : dependency_graph.edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), known_graph);
    }

    auto reverse_topo_order =
        [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {
      auto v = vector<adjacency_list<>::vertex_descriptor>(
          dependency_graph.num_vertices());

      try {
        topological_sort(known_graph, v.begin(), no_named_parameters());
      } catch (not_a_dag &e) {
        return nullopt;
      }

      return {std::move(v)};
    }();

    if (!reverse_topo_order) {
      BOOST_LOG_TRIVIAL(debug) << "conflict found in pruning";
      return false;
    }

    auto reachability = [&]() {
      auto r = vector<dynamic_bitset<>>{
          dependency_graph.num_vertices(),
          dynamic_bitset<>{dependency_graph.num_vertices()}};

      for (auto &&v : reverse_topo_order.value()) {
        r.at(v).set(v);

        for (auto &&e : as_range(out_edges(v, known_graph))) {
          auto v2 = target(e, known_graph);
          if (!r.at(v)[v2]) {
            r.at(v) |= r.at(v2);
          }
        }
      }

      return r;
    }();

    for (auto &&c : constraints | not_pruned) {
      auto check_edges = [&](const vector<Constraint::Edge> &edges) {
        for (auto &&[from, to, _] : edges) {
          if (reachability.at(vertex_map.at(to))[vertex_map.at(from)]) {
            return false;
          }
        }

        return true;
      };
      auto add_edges = [&](const vector<Constraint::Edge> &edges) {
        for (auto &&[from, to, info] : edges) {
          switch (info.type) {
            case EdgeType::WW:
              if (auto e = dependency_graph.ww.edge(from, to); e) {
                copy(info.keys, back_inserter(e.value().get().keys));
              } else {
                dependency_graph.ww.add_edge(from, to, info);
              }
              break;
            case EdgeType::RW:
              if (auto e = dependency_graph.rw.edge(from, to); e) {
                copy(info.keys, back_inserter(e.value().get().keys));
              } else {
                dependency_graph.rw.add_edge(from, to, info);
              }
              break;
            default:
              assert(false);
          }
        }
      };

      auto added_edges_name = "or";
      if (!check_edges(c.either_edges)) {
        add_edges(c.or_edges);
        changed = true;
      } else if (!check_edges(c.or_edges)) {
        add_edges(c.either_edges);
        changed = true;
        added_edges_name = "either";
      }

      if (changed) {
        pruned_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned constraint, added "
                                 << added_edges_name << " edges: " << c;
      }
    }
  }

  BOOST_LOG_TRIVIAL(debug) << "#pruned constraints: "
                           << pruned_constraints.size();

  constraints = constraints | not_pruned | to<vector<Constraint>>;

  return true;
}

}  // namespace checker::solver
