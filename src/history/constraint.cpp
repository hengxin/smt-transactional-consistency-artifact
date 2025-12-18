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
#include <cassert>

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

auto constraints_of(const InstrumentedHistory &ins_history) 
  -> std::pair<std::vector<WWConstraint>, std::vector<WRConstraint>> {
  // TODO
  return std::pair<std::vector<WWConstraint>, std::vector<WRConstraint>>{};
}

/*
auto constraints_of(const InstrumentedHistory &ins_history) 
  -> std::pair<std::vector<WWConstraint>, std::vector<WRConstraint>> {
  // 0. construct useful events(the first READ and the last WRITE per key) for each txn
  struct RWEvent {
    int64_t key;
    int64_t value; // write_value or read_values's last element
    EventType type; 
    int64_t transaction_id;
  };
  auto useful_writes = std::vector<RWEvent>{}, useful_reads = std::vector<RWEvent>{};
  {
    for (const auto &txn : ins_history.participant_txns) {
      for (const auto &[key, op] : txn.key_operations) {
        if (op.write_value) {
          useful_writes.emplace_back(RWEvent{
            .key = key,
            .value = *op.write_value,
            .type = EventType::WRITE,
            .transaction_id = txn.id,
          });
        }
        if (op.read_values) {
          useful_reads.emplace_back(RWEvent{
            .key = key,
            .value = *op.read_values->rbegin(),
            .type = EventType::READ,
            .transaction_id = txn.id,
          });
        }
      }
    }
    for (const auto &txn : ins_history.observer_txns) {
      useful_reads.emplace_back(RWEvent{
        .key = txn.key,
        .value = *txn.read_values.rbegin(),
        .type = EventType::READ,
        .transaction_id = txn.id,
      });
    }
  }
  
  // 1. add ww constraints
  auto ww_constraints = std::vector<WWConstraint>{};
  {
    auto write_txns_per_key = unordered_map<int64_t, unordered_set<int64_t>>{};
    for (const auto &event : useful_writes) {
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
    for (const auto &event : useful_reads) {
      read_events_per_txn[event.transaction_id].insert(std::make_pair(event.key, event.value));
    }
    auto txns_per_write_event = std::unordered_map<std::pair<int64_t, int64_t>, std::unordered_set<int64_t>, decltype(hash_txns_pair)>{};
    // <key, value> -> txn_id, here hash_txns_pair is borrowed.
    for (const auto &event : useful_writes) {
      txns_per_write_event[std::make_pair(event.key, event.value)].insert(event.transaction_id);
    }
    for (const auto &[read_txn_id, read_txn_events] : read_events_per_txn) {
      for (const auto &[key, value] : read_txn_events) {
        auto wr_constraint = WRConstraint { .key = key, .read_txn_id = read_txn_id, .write_txn_ids = {} };
        for (const auto &write_txn_id : txns_per_write_event[std::make_pair(key, value)]) {
          if (write_txn_id != read_txn_id) wr_constraint.write_txn_ids.insert(write_txn_id);
        }
        if (wr_constraint.write_txn_ids.empty()) {
          std::cerr << "read txn id = " << read_txn_id << ", " 
                    << "key = " << key << ", " 
                    << "value = " << value << std::endl;
          throw std::runtime_error {"exception found in construct wr constraint: no matching write value"};
        } 
        wr_constraints.emplace_back(wr_constraint);
      }
    }
  }

  BOOST_LOG_TRIVIAL(debug) << "#ww constraints: " << ww_constraints.size();
  BOOST_LOG_TRIVIAL(debug) << "#wr constraints: " << wr_constraints.size();
  BOOST_LOG_TRIVIAL(info) << "#constraints: " << ww_constraints.size() + wr_constraints.size();
  return std::make_pair(ww_constraints, wr_constraints); 
} 
*/

auto unfolded_constraints_of(const InstrumentedHistory &ins_history) -> UnfoldedConstraints {
  // TODO: unfolded constraints of 
  // 0. prepare flattened events list 
  auto keys = std::set<int64_t>{};
  auto events_per_key = std::map<int64_t, std::vector<Event>>{};
  for (const auto &txn : ins_history.participant_txns) {
    for (const auto &e : txn.events) {
      keys.insert(e.key);
      events_per_key[e.key].emplace_back(e);
    }
  }
  for (const auto &txn : ins_history.observer_txns) {
    keys.insert(txn.key);
    events_per_key[txn.key].emplace_back(Event{
      .id = txn.event_id,
      .key = txn.key,
      .read_values = txn.read_values, 
      .type = EventType::READ, 
      .transaction_id = txn.id,
    });
  }

  // 1. construct ww constraints
  auto ww_constraints = UnfoldedWWConstraints{};
  {
    for (const auto &key : keys) {
      const auto &events = events_per_key[key];
      for (auto e1_index = 0u; e1_index < events.size(); e1_index++) {
        const auto &e1 = events[e1_index];
        for (auto e2_index = e1_index + 1; e2_index < events.size(); e2_index++) {
          const auto &e2 = events[e2_index];
          if (e1.type == EventType::WRITE && e2.type == EventType::WRITE) {
            ww_constraints.emplace_back(UnfoldedWWConstraint{
              .either_event_id = e1.id, 
              .or_event_id = e2.id, 
              .key = key,
            });
          }
        }
      }
    }
  }

  // 2. construct wr constraints
  auto wr_constraints = UnfoldedWRConstraints{};
  {
    for (const auto &key : keys) {
      const auto &events = events_per_key[key];
      for (const auto &r : events) {
        if (r.type != EventType::READ) continue;
        auto write_event_ids = std::unordered_set<int64_t>{};
        for (const auto &w : events) {
          if (w.type != EventType::WRITE) continue; 
          if (w.write_value == *r.read_values.rbegin()) {
            write_event_ids.insert(w.id);
          }
        }
        if (write_event_ids.empty()) {
          std::cerr << "read txn id = " << r.transaction_id << ", " 
                    << "key = " << key << ", " 
                    << "value (of last element) = " << *r.read_values.rbegin() << std::endl;
          throw std::runtime_error {"exception found in construct wr constraint: no matching write value"};
        }
        wr_constraints.emplace_back(UnfoldedWRConstraint{
          .key = key, 
          .read_event_id = r.id, 
          .write_event_ids = write_event_ids
        });
      }
    }
  }
  
  BOOST_LOG_TRIVIAL(debug) << "#ww constraints: " << ww_constraints.size();
  BOOST_LOG_TRIVIAL(debug) << "#wr constraints: " << wr_constraints.size();
  BOOST_LOG_TRIVIAL(info) << "#constraints: " << ww_constraints.size() + wr_constraints.size();
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

auto operator<<(std::ostream &os, const UnfoldedWWConstraint &constraint)
    -> std::ostream & {
  auto out = std::osyncstream{os};
  auto print_cond = [&](const char *tag, int64_t first_id, int64_t second_id, int64_t key) {
    out << tag << ' ' << first_id << "->" << second_id << " (key = " << key << ")";
  };

  print_cond("either", constraint.either_event_id, constraint.or_event_id, constraint.key);
  out << "; ";
  print_cond("or", constraint.or_event_id, constraint.either_event_id, constraint.key);
  out << '\n';
  return os;
}


auto operator<<(std::ostream &os, const WRConstraint &constraint) 
    -> std::ostream & {
  auto out = std::osyncstream{os};
  out << "key: " << constraint.key << " { ";
  out << "read_txn: " << constraint.read_txn_id << ", "
      << "write_txns: {";
  for (const auto write_node_id : constraint.write_txn_ids) {
    out << write_node_id << ", ";
  }
  out << "} } \n";
  return os;
}

auto operator<<(std::ostream &os, const UnfoldedWRConstraint &constraint) 
    -> std::ostream & {
  auto out = std::osyncstream{os};
  out << "key: " << constraint.key << " { ";
  out << "read_event: " << constraint.read_event_id << ", "
      << "write_events: {";
  for (const auto write_node_id : constraint.write_event_ids) {
    out << write_node_id << ", ";
  }
  out << "} } \n";
  return os;
}

}  // namespace checker::history
