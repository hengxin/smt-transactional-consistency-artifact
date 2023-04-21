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
#include <optional>
#include <ranges>
#include <sstream>
#include <utility>
#include <vector>

#include "utils/literal.h"
#include "utils/to_container.h"

namespace checker::utils {

template <typename Graph>
struct CycleDetector : boost::dfs_visitor<> {
  using Vertex = typename Graph::vertex_descriptor;
  using Edge = typename Graph::edge_descriptor;

  std::reference_wrapper<std::vector<Edge>> cycle;
  std::reference_wrapper<std::vector<Vertex>> reverse_topo_order;
  std::vector<Vertex> pred;

  CycleDetector(std::reference_wrapper<std::vector<Edge>> _cycle,
                std::reference_wrapper<std::vector<Vertex>> _reverse_topo_order,
                size_t num_edges)
      : cycle{_cycle}, reverse_topo_order{_reverse_topo_order} {
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

// ref:
// http://www.doc.ic.ac.uk/%7Ephjk/Publications/DynamicTopoSortAlg-JEA-07.pdf
template <typename Graph>
static auto toposort_add_edge(
    const Graph &graph,
    std::vector<typename Graph::vertex_descriptor> &topo_order,
    typename Graph::edge_descriptor edge)
    -> std::optional<std::vector<typename Graph::edge_descriptor>> {
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
  using std::nullopt;
  using std::pair;
  using std::vector;
  using std::ranges::reverse;
  using std::ranges::sort;
  using std::ranges::subrange;
  using std::ranges::views::iota;
  using std::ranges::views::transform;

  auto vertex_pos = std::vector<size_t>{};
  vertex_pos.resize(boost::num_vertices(graph));
  for (auto i : iota(0_uz, topo_order.size())) {
    vertex_pos.at(topo_order.at(i)) = i;
  }

  auto from = source(edge, graph);
  auto to = target(edge, graph);
  auto source_pos = vertex_pos.at(from);
  auto target_pos = vertex_pos.at(to);

  if (source_pos < target_pos) {
    return nullopt;
  }

  auto filter_vertices = [&](vertex_descriptor vertex) {
    auto pos = vertex_pos.at(vertex);
    return target_pos <= pos && pos <= source_pos;
  };

  auto partial_graph = filtered_graph{
      graph,
      keep_all{},
      function{filter_vertices},
  };

  auto partial_topo_order = vector<vertex_descriptor>{};
  auto cycle = vector<edge_descriptor>{};
  auto cycle_detector = CycleDetector<decltype(partial_graph)>{
      std::ref(cycle), std::ref(partial_topo_order),
      boost::num_vertices(graph)};
  boost::depth_first_search(partial_graph, boost::visitor(cycle_detector));

  assert(!cycle.empty() || !partial_topo_order.empty());
  if (!cycle.empty()) {
    return {std::move(cycle)};
  }

  reverse(partial_topo_order);
  auto partial_vertex_pos =
      partial_topo_order                                     //
      | transform([&](auto v) { return vertex_pos.at(v); })  //
      | to_vector;
  sort(partial_vertex_pos);

  for (auto i : iota(0_uz, partial_topo_order.size())) {
    topo_order.at(partial_vertex_pos.at(i)) = partial_topo_order.at(i);
  }

  return nullopt;
}

}  // namespace checker::utils

#endif  // CHECKER_UTILS_TOPOSORT_H
