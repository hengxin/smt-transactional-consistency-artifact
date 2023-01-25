#include "dependencygraph.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/lookup_edge.hpp>
#include <cstdint>
#include <ostream>
#include <ranges>
#include <syncstream>
#include <unordered_map>
#include <utility>

#include "history.h"

using boost::add_edge;
using boost::add_vertex;
using std::pair;
using std::unordered_map;
using std::ranges::views::filter;

namespace checker::history {

DependencyGraph known_graph_of(const History &history) {
  DependencyGraph graph;
  unordered_map<int64_t, const Transaction *> transactions;

  for (const auto &txn : history.transactions()) {
    transactions.emplace(txn.id, &txn);
    graph.rw.add_vertex(txn.id);
    graph.so.add_vertex(txn.id);
    graph.wr.add_vertex(txn.id);
    graph.ww.add_vertex(txn.id);
  }

  // add SO edges
  for (const auto &sess : history.sessions) {
    const Transaction *prev_txn = nullptr;
    for (const auto &txn : sess.transactions) {
      if (prev_txn) {
        graph.so.add_edge(prev_txn->id, txn.id, EdgeInfo{.type = EdgeType::SO});
      }

      prev_txn = &txn;
    }
  }

  // add WR edges
  auto hash_pair = [](const pair<int64_t, int64_t> &p) {
    return std::hash<int64_t>{}(p.first) ^ std::hash<int64_t>{}(p.second);
  };
  std::unordered_map<pair<int64_t, int64_t>, const Transaction *,
                     decltype(hash_pair)>
      writes;
  auto filter_by_type = [](EventType t) {
    return filter([=](const auto &ev) { return ev.type == t; });
  };

  for (const auto &ev : history.events() | filter_by_type(EventType::WRITE)) {
    writes.try_emplace({ev.key, ev.value}, transactions[ev.transaction_id]);
  }

  for (const auto &ev : history.events() | filter_by_type(EventType::READ)) {
    auto write_txn = writes[{ev.key, ev.value}];
    auto txn = transactions.at(ev.transaction_id);

    if (write_txn == txn) {
      continue;
    }

    if (auto edge = graph.wr.edge(write_txn->id, txn->id); edge) {
      edge.value()->keys.emplace_back(ev.key);
    } else {
      graph.wr.add_edge(
          write_txn->id, txn->id,
          EdgeInfo{.type = EdgeType::WR, .keys = std::vector{ev.key}});
    }
  }

  return graph;
}

std::ostream &operator<<(std::ostream &os,
                         const DependencyGraph::SubGraph &graph) {
  std::osyncstream out{os};

  for (auto [from, to, info] : graph.edges()) {
    out << from << "->" << to << ' ' << *info << '\n';
  }

  return os;
}

std::ostream &operator<<(std::ostream &os, const EdgeInfo &edge_info) {
  std::osyncstream out{os};
  auto print_keys = [&] {
    out << '(';
    for (auto key : edge_info.keys) {
      out << key << ',';
    }
    out << ')';
  };

  switch (edge_info.type) {
    case EdgeType::WW:
      out << "WW";
      print_keys();
      break;
    case EdgeType::WR:
      out << "WR";
      print_keys();
      break;
    case EdgeType::RW:
      out << "RW";
      print_keys();
      break;
    case EdgeType::SO:
      out << "SO";
      break;
  }

  return os;
}

}  // namespace checker::history
