#ifndef CHECKER_UTILS_TOPOSORT_H
#define CHECKER_UTILS_TOPOSORT_H

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/log/trivial.hpp>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <sstream>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "utils/literal.h"
#include "utils/to_container.h"

namespace checker::utils {

template <typename Vertex>
struct TopologicalOrder {
  static_assert(std::is_integral_v<Vertex>);
  std::vector<Vertex> pos_to_vertex;
  std::vector<size_t> vertex_to_pos;

  explicit TopologicalOrder(const std::ranges::range auto &vertices)
      : pos_to_vertex{std::ranges::begin(vertices),
                      std::ranges::end(vertices)} {
    vertex_to_pos.resize(pos_to_vertex.size());

    for (auto i = 0_uz; i < pos_to_vertex.size(); i++) {
      vertex_to_pos.at(pos_to_vertex.at(i)) = i;
    }
  }

  auto vertex_pos(Vertex v) const -> size_t { return vertex_to_pos.at(v); }

  auto vertex_at_pos(size_t index) const -> Vertex {
    return pos_to_vertex.at(index);
  }

  auto update_pos(const std::vector<std::pair<Vertex, size_t>> &mapping) {
#ifndef NDEBUG
    auto original_pos = std::ranges::views::keys(mapping) |
                        std::ranges::views::transform(
                            [&](auto v) { return vertex_to_pos.at(v); }) |
                        to<std::unordered_set<size_t>>;

    auto new_pos =
        std::ranges::views::values(mapping) | to<std::unordered_set<size_t>>;

    assert(original_pos == new_pos);
#endif

    for (const auto &[v, i] : mapping) {
      pos_to_vertex.at(i) = v;
      vertex_to_pos.at(v) = i;
    }
  }

  auto vertices() const -> std::ranges::range auto{
    return std::ranges::views::all(pos_to_vertex);
  }
};

template <typename Graph>
struct CycleDetector : boost::dfs_visitor<> {
  using Vertex = typename Graph::vertex_descriptor;
  using Edge = typename Graph::edge_descriptor;

  std::reference_wrapper<std::vector<Edge>> cycle;
  std::reference_wrapper<std::vector<Vertex>> reverse_topo_order;
  std::vector<Vertex> pred;

  CycleDetector(std::reference_wrapper<std::vector<Edge>> cycle,
                std::reference_wrapper<std::vector<Vertex>> reverse_topo_order,
                size_t num_edges)
      : cycle{cycle}, reverse_topo_order{reverse_topo_order} {
    pred.resize(num_edges);
  }

  auto back_edge(const Edge &e, const Graph &g) -> void {
    if (!cycle.get().empty()) {
      return;
    }

    auto top = boost::target(e, g);
    auto bottom = boost::source(e, g);

    reverse_topo_order.get().clear();
    cycle.get().push_back(e);
    for (auto v = bottom; v != top; v = pred.at(v)) {
      cycle.get().push_back(boost::edge(pred.at(v), v, g).first);
    }
  }

  auto tree_edge(const Edge &e, const Graph &g) -> void {
    auto from = boost::source(e, g);
    auto to = boost::target(e, g);

    pred.at(to) = from;
  }

  auto finish_vertex(const Vertex &v, const Graph &g) -> void {
    if (cycle.get().empty()) {
      reverse_topo_order.get().emplace_back(v);
    }
  }
};

template <typename Graph>
struct IncrementalCycleDetector {
  using Edge = typename Graph::edge_descriptor;

  Graph *graph;
  TopologicalOrder<typename Graph::vertex_descriptor> topo_order;

  explicit IncrementalCycleDetector(Graph &graph)
      : graph{&graph}, topo_order{[&] {
          auto [begin, end] = boost::vertices(graph);
          return std::ranges::subrange(begin, end);
        }()} {}

  // ref:
  // http://www.doc.ic.ac.uk/%7Ephjk/Publications/DynamicTopoSortAlg-JEA-07.pdf
  auto check_add_edge(const Edge &edge) -> std::optional<std::vector<Edge>> {
    using boost::depth_first_search;
    using boost::filtered_graph;
    using boost::keep_all;
    using boost::source;
    using boost::target;
    using vertex_descriptor = typename Graph::vertex_descriptor;
    using edge_descriptor = typename Graph::edge_descriptor;
    using std::back_inserter;
    using std::function;
    using std::greater;
    using std::max;
    using std::min;
    using std::nullopt;
    using std::numeric_limits;
    using std::pair;
    using std::vector;
    using std::ranges::reverse;
    using std::ranges::sort;
    using std::ranges::subrange;
    using std::ranges::views::iota;
    using std::ranges::views::transform;

    auto from_pos = topo_order.vertex_pos(source(edge, *graph));
    auto to_pos = topo_order.vertex_pos(target(edge, *graph));
    if (from_pos < to_pos) {
      return nullopt;
    }

    auto filter_vertices = [&](vertex_descriptor vertex) {
      auto pos = topo_order.vertex_pos(vertex);
      return to_pos <= pos && pos <= from_pos;
    };

    auto partial_graph = filtered_graph{
        *graph,
        keep_all{},
        function{filter_vertices},
    };

    auto partial_topo_order = vector<vertex_descriptor>{};
    auto cycle = vector<edge_descriptor>{};
    auto cycle_detector = CycleDetector<decltype(partial_graph)>{
        std::ref(cycle), std::ref(partial_topo_order),
        boost::num_vertices(partial_graph)};
    boost::depth_first_search(partial_graph, boost::visitor(cycle_detector));

    assert(!cycle.empty() || !partial_topo_order.empty());
    if (!cycle.empty()) {
      return {std::move(cycle)};
    }

    reverse(partial_topo_order);
    auto partial_vertex_pos =
        partial_topo_order                                             //
        | transform([&](auto v) { return topo_order.vertex_pos(v); })  //
        | to_vector;
    sort(partial_vertex_pos);

    auto mapping =
        iota(0_uz, partial_topo_order.size())  //
        | transform([&](auto i) {
            return pair{partial_topo_order.at(i), partial_vertex_pos.at(i)};
          })  //
        | to_vector;
    topo_order.update_pos(mapping);

    return nullopt;
  }
};

}  // namespace checker::utils

#endif  // CHECKER_UTILS_TOPOSORT_H
