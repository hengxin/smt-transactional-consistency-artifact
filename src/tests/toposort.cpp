// clang-format off
#define BOOST_TEST_MODULE toposort
#include <boost/test/included/unit_test.hpp>
// clang-format on

#include "utils/toposort.h"

#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <unordered_set>
#include <utility>

#include "utils/to_vector.h"

using boost::add_edge;
using boost::adjacency_list;
using checker::utils::to;
using checker::utils::toposort_add_edge;
using std::array;
using std::initializer_list;
using std::pair;
using std::unordered_set;
using std::vector;
using std::ranges::equal;
using std::ranges::range;
using std::ranges::reverse;
using std::ranges::views::filter;

static auto create_graph(size_t num_vertices,
                         initializer_list<pair<size_t, size_t>> edges)
    -> pair<adjacency_list<>, vector<adjacency_list<>::vertex_descriptor>> {
  auto graph = adjacency_list<>(num_vertices);
  for (auto [from, to] : edges) {
    add_edge(from, to, graph);
  }

  auto topo_order = std::vector<adjacency_list<>::vertex_descriptor>{};
  boost::topological_sort(graph, std::back_inserter(topo_order));
  reverse(topo_order);

  return {graph, topo_order};
}

static auto partial_equal(const vector<size_t> &topo_order,
                          const vector<size_t> &partial_order) {
  auto in_partial_order =
      filter([s = partial_order | to<unordered_set<size_t>>](auto v) {
        return s.contains(v);
      });

  return equal(topo_order | in_partial_order, partial_order);
}

BOOST_AUTO_TEST_CASE(toposort) {
  auto [g, ord] = create_graph(5, {{0, 1}, {1, 3}, {2, 3}});

  auto e = add_edge(1, 2, g).first;
  BOOST_TEST(toposort_add_edge(g, ord, e));
  BOOST_TEST(partial_equal(ord, {0, 1, 2, 3}));

  auto e2 = add_edge(3, 4, g).first;
  BOOST_TEST(toposort_add_edge(g, ord, e2));
  BOOST_TEST(partial_equal(ord, {0, 1, 2, 3, 4}));

  auto e3 = add_edge(4, 1, g).first;
  BOOST_TEST(!toposort_add_edge(g, ord, e3));
}
