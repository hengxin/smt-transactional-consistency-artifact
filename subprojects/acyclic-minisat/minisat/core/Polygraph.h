#ifndef MINISAT_POLYGRAPH_H
#define MINISAT_POLYGRAPH_H

#include <cstdint>
#include <set>
#include <map>
#include <unordered_map>
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
  using WWVarInfo = std::tuple<int, int, std::vector<int64_t>>; // <from, to, keys>
  using WRVarInfo = std::tuple<int, int, int64_t>; // <from, to, key>

public:
  int n_vertices; // indexed in [0, n_vertices - 1]
  std::vector<std::pair<int, int>> known_edges;
  std::vector<std::vector<std::pair<int, int>>> edges; // if var i is assigned true, all edges in edges[i] must be added into the graph while keeping acyclicity
  
  std::unordered_map<int, WWVarInfo> ww_info;
  std::unordered_map<int, WRVarInfo> wr_info; // for solver vars

  std::unordered_map<int, std::unordered_map<int, std::set<int64_t>>> ww_keys; 
  std::unordered_map<int, std::unordered_map<int, std::set<int64_t>>> wr_keys; // for known graph

  Polygraph(int _n_vertices = 0, int _n_vars = 0) {
    n_vertices = _n_vertices;
    edges.assign(_n_vars, std::vector<std::pair<int, int>>());
  }

  void add_known_edge(int from, int to, int type, const std::vector<int64_t> &keys) { 
    known_edges.push_back(std::make_pair(from, to));
    if (type == 1) { // WW
      ww_keys[from][to].insert(keys.begin(), keys.end());
    } else if (type == 2) { // WR
      wr_keys[from][to].insert(keys.begin(), keys.end());
    }
  }

  void map_wr_var(int var, int from, int to, int64_t key) {
    assert(!wr_info.contains(var));
    wr_info[var] = WRVarInfo{from, to, key};
  }

  void map_ww_var(int var, int from, int to, const std::vector<int64_t> &keys) {
    assert(!ww_info.contains(var));
    ww_info[var] = WWVarInfo{from, to, keys};
  }

  // deprecated add_constraint_edge()
  void add_constraint_edge(int var, int from, int to) { edges[var].push_back(std::make_pair(from, to)); }
};

} // namespace Minisat

#endif // MINISAT_POLYGRAPH_H