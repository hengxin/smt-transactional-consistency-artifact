#ifndef MINISAT_POLYGRAPH_H
#define MINISAT_POLYGRAPH_H

#include <queue>
#include <vector>
#include <bitset>
#include <utility>
#include <cassert>
#include <iostream>
#include <algorithm>

#include "minisat/core/DepEdge.h"
#include "minisat/utils/Monitor.h"

namespace Minisat {

enum class EdgeType { WW, RW, WR, SO };

class Polygraph {
public:
  int n_vertices; // indexed in [0, n_vertices - 1]
  std::vector<DepEdge> known_edges;
  std::vector<std::vector<DepEdge>> edges; // if var i is assigned true, all edges in edges[i] must be added into the graph while keeping acyclicity
  
  Polygraph(int _n_vertices = 0, int _n_vars = 0) {
    n_vertices = _n_vertices;
    edges.assign(_n_vars, std::vector<DepEdge>());
  }

  void add_known_edge(int from, int to, EdgeType type) { known_edges.emplace_back((DepEdge){from, to, type}); }

  void add_constraint_edge(int var, int from, int to, EdgeType type) { edges[var].emplace_back((DepEdge){from, to, type}); }
};

} // namespace Minisat

#endif // MINISAT_POLYGRAPH_H