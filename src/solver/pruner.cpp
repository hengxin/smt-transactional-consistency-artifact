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
#include <cassert>
#include <iostream>

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
using boost::edges;
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

// prune si constraints
auto prune_constraints(DependencyGraph &dependency_graph,
                       vector<Constraint> &constraints) -> bool {
  auto n = dependency_graph.num_vertices();
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

    auto dep_graph = adjacency_list<>{n}, anti_dep_graph = adjacency_list<>{n}, known_induced_graph = adjacency_list<>{n};
    for (const auto &&[from, to, _] : dependency_graph.dep_edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), dep_graph);
      add_edge(vertex_map.at(from), vertex_map.at(to), known_induced_graph);
    }
    for (const auto &&[from, to, _] : dependency_graph.anti_dep_edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), anti_dep_graph);
    }
    for (const auto &from : as_range(vertices(dep_graph))) {
      for (const auto &&from_edge : as_range(out_edges(from, dep_graph))) {
        const auto middle = target(from_edge, dep_graph);
        for (const auto &&to_edge : as_range(out_edges(middle, anti_dep_graph))) {
          const auto to = target(to_edge, anti_dep_graph);
          add_edge(from, to, known_induced_graph);
        }
      }
    }

    auto reverse_topo_order =
        [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {


      auto v = vector<adjacency_list<>::vertex_descriptor>(n);

      try {
        topological_sort(known_induced_graph, v.begin(), no_named_parameters());
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
      auto r = vector<dynamic_bitset<>>{n, dynamic_bitset<>{n}};

      for (auto &&v : reverse_topo_order.value()) {
        r.at(v).set(v);

        for (auto &&e : as_range(out_edges(v, known_induced_graph))) {
          auto v2 = target(e, known_induced_graph);
          // FIXME: is this (!r.at(v)[v2]) condition sufficient to this dynamic programming? 
          // if (!r.at(v)[v2]) {
          //   r.at(v) |= r.at(v2);
          // }
          r.at(v) |= r.at(v2);
        }
      }

      return r;
    }();

    auto pred_edges = vector<dynamic_bitset<>>{n, dynamic_bitset<>{n}};
    for (const auto &&edge : as_range(edges(known_induced_graph))) {
      auto from = source(edge, known_induced_graph), to = target(edge, known_induced_graph);
      pred_edges.at(to).set(from);
    }

    for (auto &&c : constraints | not_pruned) {
      auto check_edges = [&](const vector<Constraint::Edge> &edges) -> bool {
        for (auto &&[from, to, info] : edges) {
          if (info.type == EdgeType::WW) {
            if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
              return false;
            }
          } else { // RW
            if ((pred_edges.at(vertex_map.at(from)) & reachability.at(to)).any()) {
              return false;
            }
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
      auto pruned = false;
      if (!check_edges(c.either_edges)) {
        add_edges(c.or_edges);
        changed = true, pruned = true;
      } else if (!check_edges(c.or_edges)) {
        add_edges(c.either_edges);
        changed = true, pruned = true;
        added_edges_name = "either";
      }

      if (pruned) {
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

// prune ser constraints
// auto prune_constraints(DependencyGraph &dependency_graph,
//                        vector<Constraint> &constraints) -> bool {
//   auto &&vertex_map = dependency_graph.rw.vertex_map.left;
//   auto pruned_constraints = unordered_set<Constraint *>{};
//   auto not_pruned =
//       filter([&](auto &&c) { return !pruned_constraints.contains(&c); });
//   bool changed = true;

//   CHECKER_LOG_COND(trace, logger) {
//     logger << "vertex_map:";
//     for (auto &&[v, desc] : vertex_map) {
//       logger << " (" << v << ", " << desc << ')';
//     }
//   }

//   while (changed) {
//     BOOST_LOG_TRIVIAL(trace) << "new pruning pass:";

//     changed = false;

//     auto known_graph = adjacency_list<>{dependency_graph.num_vertices()};
//     for (auto &&[from, to, _] : dependency_graph.edges()) {
//       add_edge(vertex_map.at(from), vertex_map.at(to), known_graph);
//     }

//     auto reverse_topo_order =
//         [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {
//       auto v = vector<adjacency_list<>::vertex_descriptor>(
//           dependency_graph.num_vertices());

//       try {
//         topological_sort(known_graph, v.begin(), no_named_parameters());
//       } catch (not_a_dag &e) {
//         return nullopt;
//       }

//       return {std::move(v)};
//     }();

//     if (!reverse_topo_order) {
//       BOOST_LOG_TRIVIAL(debug) << "conflict found in pruning";
//       return false;
//     }

//     auto reachability = [&]() {
//       auto r = vector<dynamic_bitset<>>{
//           dependency_graph.num_vertices(),
//           dynamic_bitset<>{dependency_graph.num_vertices()}};

//       for (auto &&v : reverse_topo_order.value()) {
//         r.at(v).set(v);

//         for (auto &&e : as_range(out_edges(v, known_graph))) {
//           auto v2 = target(e, known_graph);
//           // FIXME: is this !r.at(v)[v2] condition sufficient to this dynamic programming? 
//           // if (!r.at(v)[v2]) {
//           //   r.at(v) |= r.at(v2);
//           // }
//           r.at(v) |= r.at(v2);
//         }
//       }

//       return r;
//     }();

//     // for (auto &&v : reverse_topo_order.value()) std::cout << v << " ";
//     // std::cout << std::endl;

//     // for (auto &&v : reverse_topo_order.value()) {
//     //   std::cout << v << ": ";
//     //   std::cout << reachability.at(v) << std::endl;
//     // }
//     // std::cout << std::endl;

//     for (auto &&c : constraints | not_pruned) {
//       auto check_edges = [&](const vector<Constraint::Edge> &edges) -> bool {
//         for (auto &&[from, to, _] : edges) {
//           if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
//             return false;
//           }
//         }

//         return true;
//       };
//       auto add_edges = [&](const vector<Constraint::Edge> &edges) {
//         for (auto &&[from, to, info] : edges) {
//           switch (info.type) {
//             case EdgeType::WW:
//               if (auto e = dependency_graph.ww.edge(from, to); e) {
//                 copy(info.keys, back_inserter(e.value().get().keys));
//               } else {
//                 dependency_graph.ww.add_edge(from, to, info);
//               }
//               break;
//             case EdgeType::RW:
//               if (auto e = dependency_graph.rw.edge(from, to); e) {
//                 copy(info.keys, back_inserter(e.value().get().keys));
//               } else {
//                 dependency_graph.rw.add_edge(from, to, info);
//               }
//               break;
//             default:
//               assert(false);
//           }
//         }
//       };

//       auto added_edges_name = "or";
//       auto pruned = false;
//       if (!check_edges(c.either_edges)) {
//         add_edges(c.or_edges);
//         changed = true;
//         pruned = true;
//       } else if (!check_edges(c.or_edges)) {
//         add_edges(c.either_edges);
//         changed = true;
//         pruned = true;
//         added_edges_name = "either";
//       }

//       if (pruned) {
//         pruned_constraints.emplace(&c);
//         BOOST_LOG_TRIVIAL(trace) << "pruned constraint, added "
//                                  << added_edges_name << " edges: " << c;
//       }
//     }
//   }

//   BOOST_LOG_TRIVIAL(debug) << "#pruned constraints: "
//                            << pruned_constraints.size();

//   constraints = constraints | not_pruned | to<vector<Constraint>>;

//   return true;
// }

}  // namespace checker::solver
