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
  // TODO: polygraph
  using WRVarInfo = std::tuple<int, int, int64_t>; // <from, to, key>

  static const int MAX_N_VERTICES = 200000;
  std::vector<std::bitset<MAX_N_VERTICES>> reachability; 

public:
  int n_vertices; // indexed in [0, n_vertices - 1]
  int n_vars; // indexed in [0, n_vars - 1]

  std::unordered_map<int, std::unordered_set<int64_t>> write_keys_of;
  
  std::vector<std::tuple<int, int, int>> known_edges;
  
  std::unordered_map<int, WRVarInfo> wr_info; // for solver vars

  std::unordered_map<int, std::unordered_map<int, std::set<int64_t>>> wr_keys; // for known graph

  std::unordered_map<int, std::unordered_map<int, std::unordered_map<int64_t, int>>> wr_var_of; // (from, to, key) -> wr var

  std::vector<std::vector<int>> wr_cons;
  std::unordered_map<int, int> wr_cons_index_of; // var -> wr_cons index

  Polygraph(int _n_vertices = 0) { n_vertices = _n_vertices, n_vars = 0; }

  void add_known_edge(int from, int to, int type, const std::vector<int64_t> &keys) { 
    known_edges.push_back({from, to, type});
    if (type == 1) { // WR
      wr_keys[from][to].insert(keys.begin(), keys.end());
      for (const auto &key : keys) wr_var_of[from][to][key] = -1;
    }
  }

  void map_wr_var(int var, int from, int to, int64_t key) {
    assert(!wr_info.contains(var));
    wr_info[var] = WRVarInfo{from, to, key};
    wr_var_of[from][to][key] = var;
  }

  void set_n_vars(int n) { n_vars = n; }

  bool is_wr_var(int var) { return wr_info.contains(var); } // meaningless, all wr vars...

  bool has_wr_keys(int from, int to) {
    return wr_keys.contains(from) && wr_keys[from].contains(to) && !wr_keys[from][to].empty();
  }

  int add_wr_cons(std::vector<int> &cons) {
    int index = wr_cons.size();
    wr_cons.emplace_back(cons);
    return index;
  }

  void map_wr_cons(int v, int index) { wr_cons_index_of[v] = index; }

  std::vector<int> *get_wr_cons(int v) {
    if (!wr_cons_index_of.contains(v)) return nullptr;
    int index = wr_cons_index_of[v];
    return &wr_cons[index];
  }

  bool construct_known_graph_reachablity() {
    if (n_vertices > MAX_N_VERTICES) {
      std::cerr << "Failed to construct known graph reachability for n_vertices(" << n_vertices << " now)"
                << "> magic number MAX_N_VERTICES(200000 now)";
      return false;
    }
    std::vector<std::vector<int>> edges;
    edges.assign(n_vertices, std::vector<int>{});

    for (const auto &[from, to, type] : known_edges) {
      edges[from].emplace_back(to);
    }

    reachability.assign(n_vertices, std::bitset<MAX_N_VERTICES>());

    auto reversed_topo_order = [&]() {
      std::vector<int> order;
      std::vector<int> deg(n_vertices, 0);
      for (int x = 0; x < n_vertices; x++) {
        for (int y : edges[x]) {
          ++deg[y];
        }
      }
      std::queue<int> q;
      for (int x = 0; x < n_vertices; x++) {
        if (!deg[x]) { q.push(x); }
      }
      while (!q.empty()) {
        int x = q.front(); q.pop();
        order.push_back(x);
        for (int y : edges[x]) {
          --deg[y];
          if (!deg[y]) q.push(y);
        }
      }
      assert(int(order.size()) == n_vertices);
      std::reverse(order.begin(), order.end());
      return order;
    }();

    for (int x : reversed_topo_order) {
      reachability.at(x).set(x);
      for (int y : edges[x]) {
        reachability.at(x) |= reachability.at(y);
      }
    }
    return true;
  }   

  bool reachable_in_known_graph(int from, int to) { return reachability.at(from).test(to); }
};

} // namespace Minisat

#endif // MINISAT_POLYGRAPH_H