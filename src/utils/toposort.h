#ifndef CHECKER_UTILS_TOPOSORT_H
#define CHECKER_UTILS_TOPOSORT_H

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/log/trivial.hpp>
#include <functional>
#include <iterator>
#include <ranges>
#include <sstream>
#include <utility>
#include <vector>

#include "utils/to_vector.h"
#include "utils/literal.h"

namespace checker::utils {

// ref:
// http://www.doc.ic.ac.uk/%7Ephjk/Publications/DynamicTopoSortAlg-JEA-07.pdf
template <typename Graph>
static auto toposort_add_edge(
    const Graph &graph,
    std::vector<typename Graph::vertex_descriptor> &topo_order,
    typename Graph::edge_descriptor edge) -> bool {
  using boost::filtered_graph;
  using boost::not_a_dag;
  using boost::source;
  using boost::target;
  using boost::topological_sort;
  using vertex_descriptor = typename Graph::vertex_descriptor;
  using std::back_inserter;
  using std::greater;
  using std::pair;
  using std::vector;
  using std::ranges::reverse;
  using std::ranges::sort;
  using std::ranges::subrange;
  using std::ranges::views::iota;
  using std::ranges::views::transform;

  auto vertex_pos = iota(0_uz, topo_order.size())  //
                    | transform([&](auto pos) {
                        return pair{topo_order.at(pos), pos};
                      })  //
                    | to_unordered_map;

  auto from = source(edge, graph);
  auto to = target(edge, graph);
  auto source_pos = vertex_pos.at(from);
  auto target_pos = vertex_pos.at(to);

  if (source_pos < target_pos) {
    return true;
  }

  auto filter_vertices = [&](vertex_descriptor vertex) {
    auto pos = vertex_pos.at(vertex);
    return target_pos <= pos && pos <= source_pos;
  };

  auto partial_graph = filtered_graph{
      graph,
      boost::keep_all{},
      std::function{filter_vertices},
  };

  auto partial_topo_order = vector<vertex_descriptor>{};
  try {
    topological_sort(partial_graph, back_inserter(partial_topo_order));
  } catch (const not_a_dag &e) {
    return false;
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

  return true;
}

}  // namespace checker::utils

#endif  // CHECKER_UTILS_TOPOSORT_H
