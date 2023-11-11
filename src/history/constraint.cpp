#include "constraint.h"

#include <boost/log/trivial.hpp>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <ostream>
#include <ranges>
#include <syncstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "dependencygraph.h"
#include "history.h"
#include "utils/literal.h"
#include "utils/to_container.h"

using checker::utils::to;
using std::get;
using std::pair;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::ranges::subrange;
using std::ranges::views::transform;

static constexpr auto filter_write_event =
    std::ranges::views::filter([](const auto &ev) {
      return ev.type == checker::history::EventType::WRITE;
    });

static constexpr auto filter_read_event =
    std::ranges::views::filter([](const auto &ev) {
      return ev.type == checker::history::EventType::READ;
    });

static constexpr auto hash_txns_pair = [](const pair<int64_t, int64_t> &p) {
  std::hash<int64_t> h;
  return h(p.first) ^ h(p.second);
};

static constexpr auto hash_edge_endpoint = [](const auto &t) {
  auto [t1, t2, type] = t;
  std::hash<int64_t> h;
  return h(t1) ^ h(t2) ^ static_cast<decltype(h(t1))>(type);
};

namespace checker::history {
// auto constraints_of(const History &history, const DependencyGraph::SubGraph &wr)
//     -> vector<Constraint> {
//   auto write_txns_per_key = unordered_map<int64_t, unordered_set<int64_t>>{};
//   for (const auto &event : history.events() | filter_write_event) {
//     write_txns_per_key[event.key].emplace(event.transaction_id);
//   }

//   auto edges_per_txn_pair = unordered_map<
//       pair<int64_t, int64_t>,
//       unordered_map<std::tuple<int64_t, int64_t, EdgeType>, vector<int64_t>, decltype(hash_edge_endpoint)>,
//       decltype(hash_txns_pair)>{};

//   for (const auto &[key, txns] : write_txns_per_key) {
//     for (auto it = txns.begin(); it != txns.end(); it++) {
//       for (const auto &txn2 : subrange(std::next(it), txns.end())) {
//         auto add_key = [&](int64_t txn1, int64_t txn2) {
//           edges_per_txn_pair[{txn1, txn2}][{txn1, txn2, EdgeType::WW}]
//               .emplace_back(key);
//         };

//         add_key(*it, txn2);
//         add_key(txn2, *it);
//       }
//     }
//   }

//   for (const auto &a : history.transactions()) {
//     for (auto b_id : wr.successors(a.id)) {
//       const auto &keys = wr.edge(a.id, b_id).value().get().keys;
//       for (const auto &key : keys) {
//         for (auto c_id : write_txns_per_key.at(key)) {
//           if (a.id == c_id || b_id == c_id) {
//             continue;
//           }

//           edges_per_txn_pair[{a.id, c_id}][{b_id, c_id, EdgeType::RW}]
//               .emplace_back(key);
//         }
//       }
//     }
//   }

//   auto constraints = vector<Constraint>{};
//   auto added_pairs =
//       unordered_set<pair<int64_t, int64_t>, decltype(hash_txns_pair)>{};
//   for (const auto &[p, v] : edges_per_txn_pair) {
//     auto [txn1, txn2] = p;

//     if (added_pairs.contains({txn2, txn1})) {
//       continue;
//     }
//     added_pairs.emplace(txn1, txn2);

//     auto to_edge = [&](auto txn1, auto txn2) {
//       return transform([=](auto &&p) {
//         auto &&[t, keys] = p;
//         auto &&[from, to, type] = t;
//         return Constraint::Edge{
//             from,
//             to,
//             EdgeInfo{
//                 .type = type,
//                 .keys = std::move(keys),
//             },
//         };
//       });
//     };
//     auto is_ww_edge = [](auto &&e) {
//       return std::get<2>(e).type == EdgeType::WW;
//     };

//     auto either_edges = edges_per_txn_pair.at({txn1, txn2})  //
//                         | to_edge(txn1, txn2)                //
//                         | to<vector<Constraint::Edge>>;
//     auto or_edges = edges_per_txn_pair[{txn2, txn1}]  //
//                     | to_edge(txn2, txn1)             //
//                     | to<vector<Constraint::Edge>>;
//     std::swap(either_edges.front(),
//               *std::ranges::find_if(either_edges, is_ww_edge));
//     std::swap(or_edges.front(), *std::ranges::find_if(or_edges, is_ww_edge));

//     constraints.emplace_back(Constraint{
//         .either_txn_id = txn1,
//         .or_txn_id = txn2,
//         .either_edges = either_edges,
//         .or_edges = or_edges,
//     });
//   }

//   BOOST_LOG_TRIVIAL(info) << "#constraints: " << constraints.size();

//   return constraints;
// }

auto constraints_of(const History &history)
    -> std::pair<std::vector<WWConstraint>, std::vector<WRConstraint>> {
  // 1. add ww constraints
  auto ww_constraints = std::vector<WWConstraint>{};
  {
    auto write_txns_per_key = unordered_map<int64_t, unordered_set<int64_t>>{};
    for (const auto &event : history.events() | filter_write_event) {
      write_txns_per_key[event.key].emplace(event.transaction_id);
    }

    auto edges_per_txn_pair = unordered_map<
      pair<int64_t, int64_t>,
      unordered_map<std::tuple<int64_t, int64_t, EdgeType>, vector<int64_t>, decltype(hash_edge_endpoint)>,
      decltype(hash_txns_pair)>{};
    for (const auto &[key, txns] : write_txns_per_key) {
      for (auto it = txns.begin(); it != txns.end(); it++) {
        for (const auto &txn2 : subrange(std::next(it), txns.end())) {
          auto add_key = [&](int64_t txn1, int64_t txn2) {
            edges_per_txn_pair[{txn1, txn2}][{txn1, txn2, EdgeType::WW}]
                .emplace_back(key);
          };

          add_key(*it, txn2);
          add_key(txn2, *it);
        }
      }
    }

    auto added_pairs = unordered_set<pair<int64_t, int64_t>, decltype(hash_txns_pair)>{};
    for (const auto &[p, v] : edges_per_txn_pair) {
      auto [txn1, txn2] = p;

      if (added_pairs.contains({txn2, txn1})) {
        continue;
      }
      added_pairs.emplace(txn1, txn2);

      auto to_edge = [&](auto txn1, auto txn2) {
        return transform([=](auto &&p) {
          auto &&[t, keys] = p;
          auto &&[from, to, type] = t;
          return WWConstraint::Edge{
              from,
              to,
              EdgeInfo{
                  .type = type,
                  .keys = std::move(keys),
              },
          };
        });
      };

      // here, if UV constraint is relaxed, only WW edges exist
      auto either_edges = edges_per_txn_pair.at({txn1, txn2})  //
                          | to_edge(txn1, txn2)                //
                          | to<vector<WWConstraint::Edge>>;
      auto or_edges = edges_per_txn_pair[{txn2, txn1}]  //
                      | to_edge(txn2, txn1)             //
                      | to<vector<WWConstraint::Edge>>;
      ww_constraints.emplace_back(WWConstraint{
          .either_txn_id = txn1,
          .or_txn_id = txn2,
          .either_edges = either_edges,
          .or_edges = or_edges,
      });
    }
  }

  // 2. add wr constraints
  auto wr_constraints = std::vector<WRConstraint>{};
  {
    // for each read event, find all write events that write the same value for each key 
    auto read_events_per_txn = std::unordered_map<int64_t, std::unordered_set<std::pair<int64_t, int64_t>, decltype(hash_txns_pair)>>{}; // txn_id -> <key, value>
    for (const auto &event : history.events() | filter_read_event) {
      read_events_per_txn[event.transaction_id].insert(std::make_pair(event.key, event.value));
    }
    auto txns_per_write_event = std::unordered_map<std::pair<int64_t, int64_t>, std::unordered_set<int64_t>, decltype(hash_txns_pair)>{};
    // <key, value> -> txn_id, here hash_txns_pair is borrowed.
    for (const auto &event : history.events() | filter_write_event) {
      txns_per_write_event[std::make_pair(event.key, event.value)].insert(event.transaction_id);
    }
    for (const auto &[read_txn_id, read_txn_events] : read_events_per_txn) {
      for (const auto &[key, value] : read_txn_events) {
        auto wr_constraint = WRConstraint { .key = key, .read_txn_id = read_txn_id, .write_txn_ids = {} };
        for (const auto &write_txn_id : txns_per_write_event[std::make_pair(key, value)]) {
          wr_constraint.write_txn_ids.insert(write_txn_id);
        }
        wr_constraints.emplace_back(wr_constraint);
      }
    }
  }

  BOOST_LOG_TRIVIAL(info) << "#ww constraints: " << ww_constraints.size();
  BOOST_LOG_TRIVIAL(info) << "#wr constraints: " << wr_constraints.size();
  return std::make_pair(ww_constraints, wr_constraints);
}

auto operator<<(std::ostream &os, const WWConstraint &constraint)
    -> std::ostream & {
  auto out = std::osyncstream{os};
  auto print_cond = [&](const char *tag, int64_t first_id, int64_t second_id,
                        const vector<WWConstraint::Edge> &edges) {
    out << tag << ' ' << first_id << "->" << second_id << ": ";

    for (auto i = 0_uz; i < edges.size(); i++) {
      const auto &[from, to, info] = edges.at(i);
      out << from << "->" << to << ' ' << info;
      if (i != edges.size() - 1) {
        out << ", ";
      }
    }
  };

  print_cond("either", constraint.either_txn_id, constraint.or_txn_id,
             constraint.either_edges);
  out << "; ";
  print_cond("or", constraint.or_txn_id, constraint.either_txn_id,
             constraint.or_edges);
  out << '\n';
  return os;
}

auto operator<<(std::ostream &os, const WRConstraint &constraint) 
    -> std::ostream & {
  auto out = std::osyncstream{os};
  out << "key: " << constraint.key << " { ";
  out << "read_txn: " << constraint.read_txn_id << ", "
      << "write_txns: {";
  for (const auto write_txn_id : constraint.write_txn_ids) {
    out << write_txn_id << ", ";
  }
  out << "} } \n";
  return os;
}

}  // namespace checker::history
