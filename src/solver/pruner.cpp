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
#include <set>
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
using boost::target;
using boost::topological_sort;
using checker::history::WWConstraint;
using checker::history::WRConstraint;
using checker::history::Constraints;
using checker::history::DependencyGraph;
using checker::history::EdgeType;
using checker::history::EdgeInfo;
using checker::utils::as_range;
using checker::utils::to;
using std::back_inserter;
using std::nullopt;
using std::optional;
using std::unordered_set;
using std::vector;
using std::set;
using std::ranges::copy;
using std::ranges::views::filter;

namespace checker::solver {

// auto prune_constraints(DependencyGraph &dependency_graph,
//                        Constraints &constraints) -> bool {
  // auto &&vertex_map = dependency_graph.rw.vertex_map.left;
  // auto pruned_constraints = unordered_set<Constraint *>{};
  // auto not_pruned =
  //     filter([&](auto &&c) { return !pruned_constraints.contains(&c); });
  // bool changed = true;

  // CHECKER_LOG_COND(trace, logger) {
  //   logger << "vertex_map:";
  //   for (auto &&[v, desc] : vertex_map) {
  //     logger << " (" << v << ", " << desc << ')';
  //   }
  // }

  // while (changed) {
  //   BOOST_LOG_TRIVIAL(trace) << "new pruning pass:";

  //   changed = false;

  //   auto known_graph = adjacency_list<>{dependency_graph.num_vertices()};
  //   for (auto &&[from, to, _] : dependency_graph.edges()) {
  //     add_edge(vertex_map.at(from), vertex_map.at(to), known_graph);
  //   }

  //   auto reverse_topo_order =
  //       [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {
  //     auto v = vector<adjacency_list<>::vertex_descriptor>(
  //         dependency_graph.num_vertices());

  //     try {
  //       topological_sort(known_graph, v.begin(), no_named_parameters());
  //     } catch (not_a_dag &e) {
  //       return nullopt;
  //     }

  //     return {std::move(v)};
  //   }();

  //   if (!reverse_topo_order) {
  //     BOOST_LOG_TRIVIAL(debug) << "conflict found in pruning";
  //     return false;
  //   }

  //   auto reachability = [&]() {
  //     auto r = vector<dynamic_bitset<>>{
  //         dependency_graph.num_vertices(),
  //         dynamic_bitset<>{dependency_graph.num_vertices()}};

  //     for (auto &&v : reverse_topo_order.value()) {
  //       r.at(v).set(v);

  //       for (auto &&e : as_range(out_edges(v, known_graph))) {
  //         auto v2 = target(e, known_graph);
  //         // FIXME: is this !r.at(v)[v2] condition sufficient to this dynamic programming? 
  //         // if (!r.at(v)[v2]) {
  //         //   r.at(v) |= r.at(v2);
  //         // }
  //         r.at(v) |= r.at(v2);
  //       }
  //     }

  //     return r;
  //   }();

  //   // for (auto &&v : reverse_topo_order.value()) std::cout << v << " ";
  //   // std::cout << std::endl;

  //   // for (auto &&v : reverse_topo_order.value()) {
  //   //   std::cout << v << ": ";
  //   //   std::cout << reachability.at(v) << std::endl;
  //   // }
  //   // std::cout << std::endl;

  //   for (auto &&c : constraints | not_pruned) {
  //     auto check_edges = [&](const vector<Constraint::Edge> &edges) -> bool {
  //       for (auto &&[from, to, _] : edges) {
  //         if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
  //           return false;
  //         }
  //       }

  //       return true;
  //     };
  //     auto add_edges = [&](const vector<Constraint::Edge> &edges) {
  //       for (auto &&[from, to, info] : edges) {
  //         switch (info.type) {
  //           case EdgeType::WW:
  //             if (auto e = dependency_graph.ww.edge(from, to); e) {
  //               copy(info.keys, back_inserter(e.value().get().keys));
  //             } else {
  //               dependency_graph.ww.add_edge(from, to, info);
  //             }
  //             break;
  //           case EdgeType::RW:
  //             if (auto e = dependency_graph.rw.edge(from, to); e) {
  //               copy(info.keys, back_inserter(e.value().get().keys));
  //             } else {
  //               dependency_graph.rw.add_edge(from, to, info);
  //             }
  //             break;
  //           default:
  //             assert(false);
  //         }
  //       }
  //     };

  //     auto added_edges_name = "or";
  //     auto pruned = false;
  //     if (!check_edges(c.either_edges)) {
  //       add_edges(c.or_edges);
  //       changed = true;
  //       pruned = true;
  //     } else if (!check_edges(c.or_edges)) {
  //       add_edges(c.either_edges);
  //       changed = true;
  //       pruned = true;
  //       added_edges_name = "either";
  //     }

  //     if (pruned) {
  //       pruned_constraints.emplace(&c);
  //       BOOST_LOG_TRIVIAL(trace) << "pruned constraint, added "
  //                                << added_edges_name << " edges: " << c;
  //     }
  //   }
  // }

  // BOOST_LOG_TRIVIAL(debug) << "#pruned constraints: "
  //                          << pruned_constraints.size();

  // constraints = constraints | not_pruned | to<vector<Constraint>>;

  // return true;
// }

auto prune_constraints(DependencyGraph &dependency_graph,
                       Constraints &constraints) -> bool {
  // TODO: pruning
  auto &[ww_constraints, wr_constraints] = constraints;
  auto &&vertex_map = dependency_graph.rw.vertex_map.left;
  auto pruned_ww_constraints = unordered_set<WWConstraint *>{};
  auto not_pruned_ww = filter([&](auto &&c) { return !pruned_ww_constraints.contains(&c); });
  auto pruned_wr_constraints = unordered_set<WRConstraint *>{};
  auto not_pruned_wr = filter([&](auto &&c) { return !pruned_wr_constraints.contains(&c); });
  bool changed = true;

  CHECKER_LOG_COND(trace, logger) {
    logger << "vertex_map:";
    for (auto &&[v, desc] : vertex_map) {
      logger << " (" << v << ", " << desc << ')';
    }
  }

  while (changed) {
    BOOST_LOG_TRIVIAL(trace) << "new pruning pass:";

    changed = false;

    auto known_graph = adjacency_list<>{dependency_graph.num_vertices()};
    for (auto &&[from, to, _] : dependency_graph.edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), known_graph);
    }

    auto reverse_topo_order =
        [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {
      auto v = vector<adjacency_list<>::vertex_descriptor>(dependency_graph.num_vertices());

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
          // FIXME: is this !r.at(v)[v2] condition sufficient to this dynamic programming? 
          // if (!r.at(v)[v2]) {
          //   r.at(v) |= r.at(v2);
          // }
          r.at(v) |= r.at(v2);
        }
      }

      return r;
    }();

    auto keys_intersection = [](const vector<int64_t> &v1, const vector<int64_t> &v2) -> vector<int64_t> {
      auto s1 = set<int64_t>(v1.begin(), v1.end()), s2 = set<int64_t>(v2.begin(), v2.end());
      auto s3 = set<int64_t>{};
      std::set_intersection(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(s3, s3.end()));
      return vector(s3.begin(), s3.end());
    };
    auto add_edge = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
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
        case EdgeType::WR:
          if (auto e = dependency_graph.wr.edge(from, to); e) {
            copy(info.keys, back_inserter(e.value().get().keys));
          } else {
            dependency_graph.wr.add_edge(from, to, info);
          }
          break;
        default:
          assert(false);
      }
    };

    // 1. check ww constraints
    BOOST_LOG_TRIVIAL(trace) << "1. check ww constraints:";
    for (auto &&c : ww_constraints | not_pruned_ww) {
      auto check_ww_edges = [&](WWConstraint::Edge &edge) -> bool {
        const auto &[from, to, info] = edge;
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // ww edge forms a cycle
        }
        // rw edges
        for (auto &v : dependency_graph.wr.successors(from)) {
          if (v == to) continue;
          auto e = dependency_graph.wr.edge(from, v);
          assert(e);
          if (!keys_intersection(e.value().get().keys, info.keys).empty()) {
            if (reachability.at(vertex_map.at(to)).test(vertex_map.at(v))) {
              return false; // rw edge(v, to) forms a cycle
            }
          }
        }
        return true;
      };
      auto add_ww_edges = [&](WWConstraint::Edge &edge) -> void {
        auto &[from, to, info] = edge;
        add_edge(from, to, info); // ww edge
        for (auto &v : dependency_graph.wr.successors(from)) {
          if (v == to) continue;
          auto e = dependency_graph.wr.edge(from, v);
          assert(e);
          if (auto keys = keys_intersection(e.value().get().keys, info.keys); !keys.empty()) {
            // rw edge (v, to)
            add_edge(v, to, EdgeInfo{
              .type = EdgeType::RW,
              .keys = keys,
            });
          }
        }
      };

      auto add_or = !check_ww_edges(c.either_edges.at(0));
      auto add_either = !check_ww_edges(c.or_edges.at(0));
      if (add_or && add_either) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in ww pruning";
        return false;
      }

      auto added_edges_name = "or";
      auto pruned = false;
      if (add_or) {
        add_ww_edges(c.or_edges.at(0));
        changed = pruned = true;
        pruned = true;
      } else if (add_either) {
        add_ww_edges(c.either_edges.at(0));
        changed = pruned = true;
        added_edges_name = "either";
      }

      if (pruned) {
        pruned_ww_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned ww constraint, added "
                                 << added_edges_name << " edges: " << c;
      }
    }

    // 2. check wr constraints
    BOOST_LOG_TRIVIAL(trace) << "2. check wr constraints:";
    for (auto &&c : wr_constraints | not_pruned_wr) {
      auto check_wr_edges = [&](int64_t from, int64_t to, EdgeInfo info) -> bool {
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // wr edge forms a cycle
        }
        // rw edges
        for (auto v : dependency_graph.ww.successors(from)) {
          if (v == to) continue;
          auto e = dependency_graph.ww.edge(from, v);
          assert(e);
          if (auto keys = keys_intersection(e.value().get().keys, info.keys); !keys.empty()) {
            if (reachability.at(vertex_map.at(v)).test(vertex_map.at(to))) {
              return false; // rw edge (to, v) forms a cycle
            }
            // we should check (from, v), for theres a trail of from -> to -> v , 
            // however, from -> v is a WW edge, so no need
          }
        }
        return true;
      };
      auto add_wr_edges = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
        add_edge(from, to, info); // wr edge
        for (auto v : dependency_graph.ww.successors(from)) {
          if (to == v) continue;
          auto e = dependency_graph.ww.edge(from, v);
          assert(e);
          if (auto keys = keys_intersection(e.value().get().keys, info.keys); !keys.empty()) {
            // rw edge (to, v)
            add_edge(to, v, EdgeInfo{
              .type = EdgeType::RW,
              .keys = keys,
            });
          }
        }
      };

      auto &[key, read_txn_id, write_txn_ids] = c;
      auto erased_wids = vector<int64_t>{};
      for (auto write_txn_id : write_txn_ids) {
        if (!check_wr_edges(write_txn_id, read_txn_id, EdgeInfo{
            .type = EdgeType::WR,
            .keys = vector<int64_t>{key},
          })) {
          erased_wids.emplace_back(write_txn_id);
        }
      }
      for (auto id : erased_wids) write_txn_ids.erase(id);
      if (write_txn_ids.empty()) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in wr pruning:";
        CHECKER_LOG_COND(debug, logger) {
          logger << "key = " << key << "\n";
          logger << "read_txn_id = " << read_txn_id << "\n";
          logger << "write_txn_id = {";
          for (auto id : erased_wids) {
            logger << id << ", ";
          }
          logger << "}\n";
        }
        return false;
      }
      auto pruned = false;
      if (write_txn_ids.size() == 1) {
        pruned = changed = true;
        add_wr_edges(*write_txn_ids.begin(), read_txn_id, EdgeInfo{
          .type = EdgeType::WR,
          .keys = vector<int64_t>{key},
        });
      }

      if (pruned) {
        pruned_wr_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned wr constraint, added edges:" 
                                 << c;
      }
    }
  }

  ww_constraints = ww_constraints | not_pruned_ww | to<vector<WWConstraint>>;
  wr_constraints = wr_constraints | not_pruned_wr | to<vector<WRConstraint>>;
  return true;
}

}  // namespace checker::solver
