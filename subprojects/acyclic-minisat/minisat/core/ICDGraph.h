#ifndef MINISAT_ICDGRAPH_H
#define MINISAT_ICDGRAPH_H

#include <tuple>
#include <vector>
#include <bitset>
#include <queue>
#include <unordered_set>

#include "minisat/mtl/Vec.h"
#include "minisat/core/SolverTypes.h"

namespace Minisat {

constexpr static auto hash_pair = [](const auto &t) {
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

  int n, max_m, m; // n_vertices, n_edges
  std::vector<std::unordered_set<int>> in, out; // in[from], out[from] = {(to, label)}
  std::unordered_map<int, std::unordered_map<int, std::unordered_set<std::pair<int, int>, decltype(hash_pair)>>> edge_reasons;
  std::vector<int> level;
  std::vector<Lit> conflict_clause;
  std::vector<std::pair<Lit, std::vector<Lit>>> propagated_lits;

  bool detect_cycle(int from, int to, int ww_reason, int rw_reason);
  bool construct_backward_cycle(std::vector<int> &backward_pred, int from, int to, int ww_reason, int rw_reason);
  bool construct_forward_cycle(std::vector<int> &backward_pred, 
                               std::vector<int> &forward_pred, 
                               int from, int to, int ww_reason, int rw_reason, int middle);

public:
  ICDGraph();
  void init(int _n_vertices, int n_vars);
  bool add_known_edge(int from, int to, int dep_reason, int antidep_reason); // 
  bool add_edge(int from, int to, int ww_reason, int rw_reason); 
  void remove_edge(int from, int to, int ww_reason, int rw_reason); 
  void get_minimal_cycle(std::vector<Lit> &cur_conflict_clauses); // get_minimal_cycle will copy conflict_clause and clear it
};
}

#endif // MINISAT_ICDGRAPH_H