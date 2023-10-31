#ifndef MINISAT_ICDGRAPH_H
#define MINISAT_ICDGRAPH_H

#include <tuple>
#include <vector>
#include <unordered_set>

#include "minisat/mtl/Vec.h"
#include "minisat/core/SolverTypes.h"

namespace Minisat {

constexpr static auto edge_info_hash_endpoint = [](const auto &t) {
  auto &[t1, t2] = t;
  std::hash<int> h;
  return h(t1) ^ h(t2); 
};

class ICDGraph {
  /* implement a graph data structure, to support below operations:
    1. add an edge / remove an edge
    2. detect cycles
    3. find the minimal cycle
  */ 
  using EdgeInfo = std::pair<int, int>;
  using ICDGraphEdge = std::tuple<int, int, int>; // (from, to, label), in this case, the edges are all distinct

  int n, max_m, m; // n_vertices, n_edges
  std::vector<std::unordered_set<std::pair<int, int>, decltype(edge_info_hash_endpoint)>> in, out; // in[from], out[from] = {(to, label)}
  std::vector<std::unordered_set<std::pair<int, int>, decltype(edge_info_hash_endpoint)>> inactive_edges; // same as in, out
  std::vector<int> level;
  std::vector<Lit> conflict_clause;
  std::vector<Lit> propagated_lits;

  bool detect_cycle(int from, int to, int label);
  bool construct_backward_cycle(std::vector<EdgeInfo> &backward_pred, int from, int to, int label);
  bool construct_forward_cycle(std::vector<EdgeInfo> &backward_pred, 
                               std::vector<EdgeInfo> &forward_pred, 
                               int from, int to, int label, int middle);
  void construct_propagated_lits(std::unordered_set<int> &forward_visited, std::unordered_set<int> &backward_visited);

public:
  ICDGraph();
  void init(int _n_vertices);
  void add_inactive_edge(int from, int to, int label);
  bool add_edge(int from, int to, int label, bool need_detect_cycle = true); // add (from, to, label)
  void remove_edge(int from, int to, int label); // remove (from, to, label), assume (from, to, label) in the graph
  void get_minimal_cycle(std::vector<Lit> &cur_conflict_clauses); // get_minimal_cycle will copy conflict_clause and clear it
  void get_propagated_lits(std::vector<Lit> &cur_propagated_lit); // get_propagated_lits will copy propagated_lits and clear it 
};
}

#endif // MINISAT_ICDGRAPH_H