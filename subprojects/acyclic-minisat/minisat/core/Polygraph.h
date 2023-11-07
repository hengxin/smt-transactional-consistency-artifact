#ifndef MINISAT_POLYGRAPH_H
#define MINISAT_POLYGRAPH_H

#include <queue>
#include <vector>
#include <bitset>
#include <utility>
#include <cassert>
#include <iostream>
#include <algorithm>

#include "minisat/utils/Monitor.h"

namespace Minisat {

class Polygraph {
public:
  int n_vertices; // indexed in [0, n_vertices - 1]
  std::vector<std::pair<int, int>> known_edges;
  std::vector<std::vector<std::pair<int, int>>> edges; // if var i is assigned true, all edges in edges[i] must be added into the graph while keeping acyclicity
  
  Polygraph(int _n_vertices = 0, int _n_vars = 0) {
    n_vertices = _n_vertices;
    edges.assign(_n_vars, std::vector<std::pair<int, int>>());
  }

  void add_known_edge(int from, int to) { known_edges.push_back(std::make_pair(from, to)); }

  void rebuild_known_edges() {
    static const int MAX_N_VERTICES = 100000;
    auto reachablity = std::vector<std::bitset<MAX_N_VERTICES>>(n_vertices, std::bitset<MAX_N_VERTICES>());
    auto graph = std::vector<std::vector<int>>(n_vertices, std::vector<int>());
    for (const auto &[from, to] : known_edges) graph[from].push_back(to);
    auto reversed_topo_order = [&]() {
      std::vector<int> order;
      std::vector<int> deg(n_vertices, 0);
      for (int x = 0; x < n_vertices; x++) {
        for (int y : graph[x]) ++deg[y];
      }
      std::queue<int> q;
      for (int x = 0; x < n_vertices; x++) {
        if (!deg[x]) q.push(x);
      }
      while (!q.empty()) {
        int x = q.front(); q.pop();
        order.push_back(x);
        for (int y : graph[x]) {
          --deg[y];
          if (!deg[y]) q.push(y);
        }
      }
      assert(int(order.size()) == n_vertices);
      std::reverse(order.begin(), order.end());
      return std::move(order);
    }();

    for (int x : reversed_topo_order) {
      reachablity.at(x).set(x);
      for (int y : graph[x]) {
        reachablity.at(x) |= reachablity.at(y);
      }
    }

    std::vector<std::pair<int, int>> new_known_edges;
    for (int x = 0; x < n_vertices; x++) {
      for (int y = 0; y < n_vertices; y++) {
        if (x == y) continue;
        if (reachablity.at(x).test(y)) {
          new_known_edges.push_back(std::make_pair(x, y));
        } 
      }
    }
    std::swap(known_edges, new_known_edges);
    std::cerr << "size of new_known_edges: " << known_edges.size() << std::endl;
  }

  void add_constraint_edge(int var, int from, int to) { edges[var].push_back(std::make_pair(from, to)); }
};

} // namespace Minisat

#endif // MINISAT_POLYGRAPH_H