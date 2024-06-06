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

auto constraints_of(const History &history) -> std::vector<WRConstraint> {
  // 0. construct useful events(the first READ and the last WRITE per key) for each txn
  auto useful_writes = std::vector<Event>{}, useful_reads = std::vector<Event>{};
  {
    for (const auto &session : history.sessions) {
      for (const auto &txn : session.transactions) {
        auto cur_value = std::unordered_map<int64_t, int64_t>{}; // key -> value (for current txn)
        for (const auto &event : txn.events) {
          const auto &[key, value, type, txn_id] = event;
          if (type == EventType::READ) {
            if (!cur_value.contains(key)) {
              useful_reads.emplace_back(event);
            } else {
              if (cur_value[key] != value) 
                throw std::runtime_error{"exception found in 1 txn."}; // violate INT
            }
          } else { // EventType::WRITE
            cur_value[key] = value;
          }
        }
        for (const auto &[key, value] : cur_value) {
          useful_writes.emplace_back((Event) {
            .key = key, 
            .value = value, 
            .type = EventType::WRITE, 
            .transaction_id = txn.id
          });
        }
      }
    }
  }

  // 1. add wr constraints
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
          throw std::runtime_error {"exception found in construct wr constraint: no matched value write"};
        } 
        wr_constraints.emplace_back(wr_constraint);
      }
    }
  }

  BOOST_LOG_TRIVIAL(debug) << "#constraints: " << wr_constraints.size();
  return wr_constraints;
}

auto measuring_repeat_values(const Constraints &constraints) -> void {
  const auto &wr_cons = constraints;
  auto n_wr_cons = 0u;
  auto n_unit_wr_cons = 0u;
  auto max_wr_length = 0u;
  auto sum_wr_length = 0u;
  for (const auto &[key, read, writes] : wr_cons) {
    ++n_wr_cons;
    if (writes.size() == 1u) ++n_unit_wr_cons;
    max_wr_length = std::max(max_wr_length, (unsigned)writes.size());
    sum_wr_length += (unsigned)writes.size();
  }

  if (n_wr_cons == n_unit_wr_cons) {
    BOOST_LOG_TRIVIAL(debug) << "This history may not satisfy UniqueValue. But there's no WR constraint.";
  } else {
    BOOST_LOG_TRIVIAL(debug) << "#unit wr constraints = " << n_unit_wr_cons;
    BOOST_LOG_TRIVIAL(debug) << "unit wr ratio = " << 1.0 * n_unit_wr_cons / n_wr_cons;
    BOOST_LOG_TRIVIAL(debug) << "max wr length = " << max_wr_length;
    BOOST_LOG_TRIVIAL(debug) << "average wr length = " << 1.0 * sum_wr_length / n_wr_cons;
    BOOST_LOG_TRIVIAL(debug) << "average wr length(w/o unit cons) = " << 1.0 * (sum_wr_length - n_unit_wr_cons) / (n_wr_cons - n_unit_wr_cons);
  }

  // count encoding size
  int64_t some_count = 0, unique_count = 0;
  some_count += wr_cons.size();
  for (const auto &[key, read, writes] : wr_cons) {
    unique_count += 1LL * writes.size() * (writes.size() - 1) / 2;
  }
  BOOST_LOG_TRIVIAL(debug) << "#some constraint: " << some_count;
  BOOST_LOG_TRIVIAL(debug) << "#unique constraint: " << unique_count;
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
