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

enum class EdgeType { WW, RW, WR, SO };

struct EdgeInfo {
  uint64_t id;
  EdgeType type;
  std::vector<int64_t> keys;

  friend auto operator<<(std::ostream &os, const EdgeInfo &edge_info)
      -> std::ostream &;

  friend auto operator==(const EdgeInfo &left, const EdgeInfo &right) -> bool;
};

struct DependencyGraph {
  using SubGraph = utils::Graph<int64_t, EdgeInfo>;

  SubGraph so;
  SubGraph rw;
  SubGraph wr;
  SubGraph ww;

  auto edges() const -> std::ranges::range auto{
    return std::array{so.edges(), rw.edges(), wr.edges(), ww.edges()}  //
           | std::ranges::views::join;
  }
};

auto known_graph_of(const History &history) -> DependencyGraph;

}  // namespace checker::history

#endif /* CHECKER_HISTORY_DEPENDENCYGRAPH_H */
