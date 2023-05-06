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

using std::get;
using std::pair;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::ranges::subrange;
using std::ranges::views::transform;

using checker::utils::to_vector;

static constexpr auto filter_write_event =
    std::ranges::views::filter([](const auto &ev) {
      return ev.type == checker::history::EventType::WRITE;
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
auto constraints_of(const History &history, const DependencyGraph::SubGraph &wr)
    -> vector<Constraint> {
  auto write_txns_per_key = unordered_map<int64_t, unordered_set<int64_t>>{};
  for (const auto &event : history.events() | filter_write_event) {
    write_txns_per_key[event.key].emplace(event.transaction_id);
  }

  auto edges_per_txn_pair = unordered_map<
      pair<int64_t, int64_t>,
      unordered_map<std::tuple<int64_t, int64_t, EdgeType>, vector<int64_t>,
                    decltype(hash_edge_endpoint)>,
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

  for (const auto &a : history.transactions()) {
    for (auto b_id : wr.successors(a.id)) {
      const auto &keys = wr.edge(a.id, b_id).value().get().keys;
      for (const auto &key : keys) {
        for (auto c_id : write_txns_per_key.at(key)) {
          if (a.id == c_id || b_id == c_id) {
            continue;
          }

          edges_per_txn_pair[{a.id, c_id}][{b_id, c_id, EdgeType::RW}]
              .emplace_back(key);
        }
      }
    }
  }

  auto constraints = vector<Constraint>{};
  auto added_pairs =
      unordered_set<pair<int64_t, int64_t>, decltype(hash_txns_pair)>{};
  auto edge_id = 0_u64;
  for (const auto &[p, v] : edges_per_txn_pair) {
    auto [txn1, txn2] = p;

    if (added_pairs.contains({txn2, txn1})) {
      continue;
    }
    added_pairs.emplace(txn1, txn2);

    auto to_edge = [&](auto txn1, auto txn2) {
      return transform([=, &edge_id](auto &&p) {
        auto &&[t, keys] = p;
        auto &&[from, to, type] = t;
        return Constraint::Edge{
            from,
            to,
            EdgeInfo{
                .id = edge_id++,
                .type = type,
                .keys = std::move(keys),
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

  BOOST_LOG_TRIVIAL(info) << "#constraints: " << constraints.size();

  return constraints;
}

auto operator<<(std::ostream &os, const Constraint &constraint)
    -> std::ostream & {
  auto out = std::osyncstream{os};
  auto print_cond = [&](const char *tag, int64_t first_id, int64_t second_id,
                        const vector<Constraint::Edge> &edges) {
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

}  // namespace checker::history
