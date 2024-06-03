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
    case EdgeType::LO:
      out << "LO";
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
}  // namespace checker::history
