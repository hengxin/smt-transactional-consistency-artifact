#ifndef CHECKER_HISTORY_DEPENDENCYGRAPH_H
#define CHECKER_HISTORY_DEPENDENCYGRAPH_H

#include <array>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <optional>
#include <ranges>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "history.h"
#include "utils/graph.h"

namespace checker::history {

enum class EdgeType { WW, RW, WR, SO, LO };

struct EdgeInfo {
  EdgeType type;
  std::vector<int64_t> keys; // len(keys) == 0 or 1

  friend auto operator<<(std::ostream &os, const EdgeInfo &edge_info)
      -> std::ostream &;
};

struct DependencyGraph {
  using SubGraph = utils::Graph<int64_t, EdgeInfo>;

  SubGraph so;
  SubGraph rw;
  SubGraph wr;
  SubGraph ww;
  SubGraph lo;

  auto edges() const -> std::ranges::range auto{
    return std::array{so.edges(), rw.edges(), wr.edges(), ww.edges(), lo.edges()}  //
           | std::ranges::views::join;
  }

  auto num_vertices() const -> size_t { return boost::num_vertices(*so.graph); }

  // TODO: SI checking theorm
  auto dep_edges() const -> std::ranges::range auto { return std::array{so.edges(), wr.edges(), ww.edges()} | std::ranges::views::join; }
  auto anti_dep_edges() const -> std::ranges::range auto { return std::array{rw.edges()} | std::ranges::views::join; }

  friend auto operator<<(std::ostream &os, const DependencyGraph &graph)
      -> std::ostream &;
};

auto known_graph_of(const History &history, const HistoryMetaInfo &history_meta) -> DependencyGraph;
auto known_graph_of(const InstrumentedHistory &ins_history) -> DependencyGraph;

}  // namespace checker::history

#endif /* CHECKER_HISTORY_DEPENDENCYGRAPH_H */
