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

auto known_graph_of(const History &history) -> DependencyGraph {
  auto graph = DependencyGraph{};
  auto transactions = unordered_map<int64_t, const Transaction *>{};

  for (const auto &txn : history.transactions()) {
    transactions.emplace(txn.id, &txn);

    for (auto subgraph : {&graph.so, &graph.wr, &graph.co}) {
      subgraph->add_vertex(txn.id);
    }
  }

  // add SO edges
  for (const auto &sess : history.sessions) {
    auto prev_txn = (const Transaction *){};
    for (const auto &txn : sess.transactions) {
      if (prev_txn) {
        graph.so.add_edge(prev_txn->id, txn.id, EdgeInfo{.type = EdgeType::SO});
      }

      prev_txn = &txn;
    }
  }
  return graph; // if UniqueValue constraint is relaxed, known graph only constains SO edges.
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
    case EdgeType::CO:
      out << "CO";
      print_keys();
      break;
    case EdgeType::WR:
      out << "WR";
      print_keys();
      break;
    case EdgeType::SO:
      out << "SO";
      break;
  }

  return os;
}

auto operator<<(std::ostream &os, const DependencyGraph &graph)
    -> std::ostream & {
  auto out = std::osyncstream{os};
  out << "SO:\n"
      << graph.so << '\n'
      << "WR:\n"
      << graph.wr << '\n'
      << "CO:\n"
      << graph.co << '\n';

  return os;
}
}  // namespace checker::history
