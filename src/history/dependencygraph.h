#ifndef CHECKER_HISTORY_DEPENDENCYGRAPH_H
#define CHECKER_HISTORY_DEPENDENCYGRAPH_H

#include <array>
#include <boost/graph/adjacency_list.hpp>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <ranges>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "history.h"

namespace checker::history {

enum class EdgeType { WW, RW, WR, SO };

struct EdgeInfo {
  EdgeType type;
  std::vector<int64_t> keys;

  friend std::ostream &operator<<(std::ostream &os, const EdgeInfo &edge_info);
};

struct DependencyGraph {
  struct SubGraph {
    using GraphStorage =
        boost::adjacency_list<boost::hash_setS, boost::vecS, boost::directedS,
                              int64_t, EdgeInfo>;

    GraphStorage graph;
    std::unordered_map<int64_t, GraphStorage::vertex_descriptor> vertex_map;

    void add_vertex(int64_t v) {
      auto desc = boost::add_vertex(v, graph);
      vertex_map.emplace(v, desc);
    }

    void add_edge(int64_t from, int64_t to, EdgeInfo &&info) {
      boost::add_edge(vertex_map.at(from), vertex_map.at(to), std::move(info),
                      graph);
    }

    std::optional<EdgeInfo *> edge(int64_t from, int64_t to) {
      if (auto r = std::as_const(*this).edge(from, to); r) {
        return std::optional{const_cast<EdgeInfo *>(r.value())};
      } else {
        return std::nullopt;
      }
    }

    std::optional<const EdgeInfo *> edge(int64_t from, int64_t to) const {
      auto result = boost::edge(vertex_map.at(from), vertex_map.at(to), graph);

      if (result.second) {
        return std::optional{&graph[result.first]};
      } else {
        return std::nullopt;
      }
    }

    auto successors(int64_t vertex) const {
      auto [begin, end] = boost::out_edges(vertex_map.at(vertex), graph);
      return std::ranges::subrange(begin, end)  //
             | std::ranges::views::transform([&](auto desc) {
                 return graph[boost::target(desc, graph)];
               });
    }

    auto vertices() const {
      auto [begin, end] = boost::vertices(graph);
      return std::ranges::subrange(begin, end)  //
             | std::ranges::views::transform(
                   [&](auto desc) { return graph[desc]; });
    }

    auto edges() const {
      auto [begin, end] = boost::edges(graph);
      return std::ranges::subrange(begin, end)  //
             | std::ranges::views::transform([&](auto desc) {
                 return std::tuple{
                     graph[boost::source(desc, graph)],
                     graph[boost::target(desc, graph)],
                     &graph[desc],
                 };
               });
    }

    friend std::ostream &operator<<(std::ostream &os, const SubGraph &graph);
  };

  SubGraph so;
  SubGraph rw;
  SubGraph wr;
  SubGraph ww;

  auto edges() const {
    return std::array{so.edges(), rw.edges(), wr.edges(), ww.edges()}  //
           | std::ranges::views::join;
  }
};

DependencyGraph known_graph_of(const History &history);

}  // namespace checker::history

#endif /* CHECKER_HISTORY_DEPENDENCYGRAPH_H */
