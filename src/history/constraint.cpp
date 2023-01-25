#include "constraint.h"

#include <cstdint>
#include <functional>
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
#include "utils/to_vector.h"

using std::get;
using std::pair;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::ranges::subrange;
using std::ranges::views::transform;

using checker::utils::to_vector;

static constexpr auto filter_read_event =
    std::ranges::views::filter([](const auto &ev) {
      return ev.type == checker::history::EventType::READ;
    });

static constexpr auto hash_txns_pair = [](const pair<int64_t, int64_t> &p) {
  std::hash<int64_t> h;
  return h(p.first) ^ h(p.second);
};

namespace checker::history {
vector<Constraint> constraints_of(const History &history,
                                  const DependencyGraph::SubGraph &wr) {
  unordered_map<int64_t, unordered_set<int64_t>> write_txns_per_key;
  for (const auto &event : history.events() | filter_read_event) {
    write_txns_per_key[event.key].emplace(event.transaction_id);
  }

  unordered_map<pair<int64_t, int64_t>,
                unordered_map<EdgeType, vector<int64_t>>,
                decltype(hash_txns_pair)>
      edges_per_txn_pair;
  for (const auto &[key, txns] : write_txns_per_key) {
    for (auto it = txns.begin(); it != txns.end(); it++) {
      for (const auto &txn2 : subrange(std::next(it), txns.end())) {
        auto add_key = [&](int64_t txn1, int64_t txn2) {
          edges_per_txn_pair[{txn1, txn2}][EdgeType::WW].emplace_back(key);
        };

        add_key(*it, txn2);
        add_key(txn2, *it);
      }
    }
  }

  for (const auto &a : history.transactions()) {
    for (auto b_id : wr.successors(a.id)) {
      const auto &keys = wr.edge(a.id, b_id).value()->keys;
      for (const auto &key : keys) {
        for (auto c_id : write_txns_per_key.at(key)) {
          if (a.id == c_id || b_id == c_id) {
            continue;
          }

          edges_per_txn_pair[{a.id, c_id}][EdgeType::RW].emplace_back(key);
        }
      }
    }
  }

  vector<Constraint> constraints;
  unordered_set<pair<int64_t, int64_t>, decltype(hash_txns_pair)> added_pairs;
  for (const auto &[p, v] : edges_per_txn_pair) {
    auto [txn1, txn2] = p;

    if (added_pairs.contains({txn2, txn1})) {
      continue;
    }
    added_pairs.emplace(txn1, txn2);

    auto to_edge = [](auto txn1, auto txn2) {
      return transform([=](auto p) {
        return Constraint::Edge{
            txn1,
            txn2,
            EdgeInfo{
                .type = p.first,
                .keys = std::move(p.second),
            },
        };
      });
    };
    constraints.emplace_back(Constraint{
        .either_txn_id = txn1,
        .or_txn_id = txn2,
        .either_edges = edges_per_txn_pair[{txn1, txn2}]  //
                        | to_edge(txn1, txn2) | to_vector,
        .or_edges = edges_per_txn_pair[{txn2, txn1}]  //
                    | to_edge(txn2, txn1) | to_vector,
    });
  }

  return constraints;
}

std::ostream &operator<<(std::ostream &os, const Constraint &constraint) {
  std::osyncstream out{os};
  auto print_cond = [&](const char *tag, int64_t first_id, int64_t second_id,
                        const vector<Constraint::Edge> &edges) {
    out << tag << ": " << first_id << "->" << second_id << ' ';

    for (const auto &edge : edges) {
      out << get<0>(edge) << "->" << get<1>(edge) << ' ' << get<2>(edge)
          << ", ";
    }

    out << "; ";
  };

  print_cond("either", constraint.either_txn_id, constraint.or_txn_id, constraint.either_edges);
  print_cond("or", constraint.or_txn_id, constraint.either_txn_id, constraint.or_edges);
  out << '\n';

  return os;
}

}  // namespace checker::history
