#include "dependencygraph.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/lookup_edge.hpp>
#include <cstdint>
#include <ostream>
#include <ranges>
#include <syncstream>
#include <iostream>
#include <unordered_map>
#include <utility>

#include "history.h"
#include "utils/literal.h"
#include "utils/log.h"

using boost::add_edge;
using boost::add_vertex;
using std::pair;
using std::unordered_map;
using std::ranges::views::filter;

namespace checker::history {

auto known_graph_of(const History &history, const HistoryMetaInfo &history_meta) -> DependencyGraph {
  // TODO: construct known graph, including SO and PO
  const auto &[
    n_sess,
    n_total_txns,
    n_total_evts,
    n_nodes,
    begin_node,
    end_node,
    write_node,
    read_node,
    event_value,
    read_length
  ] = history_meta;
  auto graph = DependencyGraph{};

  for (int64_t i = 0; i < n_nodes; i++) {
    for (auto subgraph : {&graph.rw, &graph.so, &graph.wr, &graph.ww, &graph.lo}) {
      subgraph->add_vertex(i);
    }
  }

  // 1. add SO edges
  for (const auto &sess : history.sessions) {
    auto prev_txn = (const Transaction *){};
    for (const auto &txn : sess.transactions) {
      if (prev_txn) {
        graph.so.add_edge(end_node.at(prev_txn->id), begin_node.at(txn.id), EdgeInfo{.type = EdgeType::SO});
      }

      prev_txn = &txn;
    }
  }

  // 2. add PO edges
  for (const auto &txn : history.transactions()) {
    int64_t begin = begin_node.at(txn.id), end = end_node.at(txn.id);
    for (int64_t node = begin; node < end; ++node) {
      graph.lo.add_edge(node, node + 1, EdgeInfo{.type = EdgeType::LO});
    }
  }
  return graph; 
  // if UniqueValue constraint is relaxed, known graph only constains SO edges.
  // In the non-UniqueValue list append problem, known graph contains PO edges as well.
}

auto known_graph_of(const InstrumentedHistory &ins_history) -> DependencyGraph {
  auto graph = DependencyGraph{};

  // 1. add vertex(txn id)
  for (const auto &txn : ins_history.participant_txns) {
    for (auto subgraph : {&graph.rw, &graph.so, &graph.wr, &graph.ww, &graph.lo}) {
      subgraph->add_vertex(txn.id);
    } 
  }
  for (const auto &txn : ins_history.observer_txns) {
    for (auto subgraph : {&graph.rw, &graph.so, &graph.wr, &graph.ww, &graph.lo}) {
      subgraph->add_vertex(txn.id);
    } 
  }

  // 2. add SO and LO edges
  for (const auto &[from, to] : ins_history.so_orders) {
    graph.so.add_edge(from, to, EdgeInfo{.type = EdgeType::SO});
  }
  for (const auto &[from, to] : ins_history.lo_orders) {
    graph.lo.add_edge(from, to, EdgeInfo{.type = EdgeType::LO});
  }

  return graph;
}

auto unfolded_known_graph_of(const InstrumentedHistory &ins_history) -> UnfoldedDependencyGraph {
  auto graph = UnfoldedDependencyGraph{};

  // 1. add vertex(event id)
  for (const auto &txn : ins_history.participant_txns) {
    for (const auto &e : txn.events) {
      for (auto subgraph : {&graph.rw, &graph.so, &graph.wr, &graph.ww, &graph.lo, &graph.po}) {
        subgraph->add_vertex(e.id);
      } 
    }
  }
  for (const auto &txn : ins_history.observer_txns) {
    for (auto subgraph : {&graph.rw, &graph.so, &graph.wr, &graph.ww, &graph.lo, &graph.po}) {
      subgraph->add_vertex(txn.event_id);
    } 
  }

  // 2. add po, so, lo 
  for (const auto &txn : ins_history.participant_txns) {
    for (auto e1_index = 0u; e1_index < txn.events.size(); e1_index++) {
      auto &e1 = txn.events[e1_index];
      for (auto e2_index = e1_index + 1; e2_index < txn.events.size(); e2_index++) {
        auto &e2 = txn.events[e2_index];
        graph.po.add_edge(e1.id, e2.id, EdgeInfo{.type = EdgeType::PO}); 
      }
    }
  }

  auto participant_txn_of = std::map<int64_t, ParticipantTransaction>{};
  for (const auto &txn : ins_history.participant_txns) {
    assert(!participant_txn_of.contains(txn.id));
    participant_txn_of[txn.id] = txn;
  }
  for (const auto &[from, to] : ins_history.so_orders) {
    graph.so.add_edge(
      participant_txn_of[from].events.rbegin()->id, 
      participant_txn_of[to].events.begin()->id,
      EdgeInfo{.type = EdgeType::SO}
    ); 
  }

  auto observer_event_id_of = std::map<int64_t, int64_t>{}; // txn id -> event id, for observer txns
  for (const auto &txn : ins_history.observer_txns) {
    assert(!observer_event_id_of.contains(txn.id));
    observer_event_id_of[txn.id] = txn.event_id;
  } 
  for (const auto &[from, to] : ins_history.lo_orders) {
    graph.lo.add_edge(
      observer_event_id_of.at(from), 
      observer_event_id_of.at(to), 
      EdgeInfo{.type = EdgeType::LO} 
    );
  }
  return graph;
}

auto operator<<(std::ostream &os, const EdgeInfo &edge_info) -> std::ostream & {
  auto out = std::osyncstream{os};
  auto print_keys = [&] {
    out << '(';

    const auto &keys = edge_info.keys;
    for (auto i = 0_uz; i < keys.size(); i++) {
      out << keys.at(i);
      if (i != keys.size() - 1) {
        out << ' ';
      }
    }

    out << ')';
  };

  switch (edge_info.type) {
    case EdgeType::WW:
      out << "ww";
      print_keys();
      break;
    case EdgeType::WR:
      out << "wr";
      print_keys();
      break;
    case EdgeType::RW:
      out << "rw";
      print_keys();
      break;
    case EdgeType::SO:
      out << "so";
      break;
    case EdgeType::LO:
      out << "lo";
      break;
    case EdgeType::PO:
      out << "po";
      break;
  }

  return os;
}

auto operator<<(std::ostream &os, const DependencyGraph &graph)
    -> std::ostream & {
  auto out = std::osyncstream{os};
  out << "RW:\n"
      << graph.rw << '\n'
      << "WW:\n"
      << graph.ww << '\n'
      << "SO:\n"
      << graph.so << '\n'
      << "WR:\n"
      << graph.wr << '\n'
      << "LO:\n"
      << graph.lo << '\n';

  return os;
}

auto operator<<(std::ostream &os, const UnfoldedDependencyGraph &graph)
    -> std::ostream & {
  auto out = std::osyncstream{os};
  out << "rw:\n"
      << graph.rw << '\n'
      << "ww:\n"
      << graph.ww << '\n'
      << "so:\n"
      << graph.so << '\n'
      << "wr:\n"
      << graph.wr << '\n'
      << "lo:\n"
      << graph.lo << '\n'
      << "po:\n"
      << graph.po << '\n';

  return os;
}
}  // namespace checker::history
