#ifndef MINISAT_ICDGRAPH_H
#define MINISAT_ICDGRAPH_H

#include <tuple>
#include <vector>
#include <queue>
#include <unordered_set>
#include <unordered_map>

#include "minisat/mtl/Vec.h"
#include "minisat/core/SolverTypes.h"

namespace Minisat {

constexpr static auto pair_hash_endpoint2 = [](const auto &t) {
  auto &[t1, t2] = t;
  std::hash<int> h;
  return h(t1) ^ h(t2); 
};

constexpr int MAX_N = 100000;

class ICDGraph {
  /* implement a graph data structure, to support below operations:
    1. add an edge / remove an edge
    2. detect cycles
    3. find the minimal cycle
  */ 
  
  int n, max_m, m; // n_vertices, n_edges
  std::vector<std::unordered_set<int>> in, out; // in[from], out[from] = {(to, label)}
  std::unordered_map<int, std::unordered_map<int, std::unordered_set<std::pair<int, int>, decltype(pair_hash_endpoint2)>>> reasons_of; // (from, to) -> {(ww_reason, wr_reason)}
  std::vector<int> level;
  std::vector<Lit> conflict_clause;
  std::vector<std::pair<Lit, std::vector<Lit>>> propagated_lits;

  // --- deprecated ---
  int check_cnt = 0;
  std::vector<bool> is_var_unassigned;
  std::queue<int> vars_queue;
  // ------------------

  bool check_acyclicity();
  bool detect_cycle(int from, int to, std::pair<int, int> reason);
  bool construct_backward_cycle(std::vector<int> &backward_pred, int from, int to, std::pair<int, int> reason);
  bool construct_forward_cycle(std::vector<int> &backward_pred, 
                               std::vector<int> &forward_pred, 
                               int from, int to, std::pair<int, int> reason, int middle);
  void construct_propagated_lits(std::unordered_set<int> &forward_visited, 
                                 std::unordered_set<int> &backward_visited,
                                 std::vector<int> &forward_pred,
                                 std::vector<int> &backward_pred,
                                 int from, int to, std::pair<int, int> reason);

public:
  ICDGraph();
  void init(int _n_vertices, int n_vars);
  void add_inactive_edge(int from, int to, std::pair<int, int> reason);
  bool add_known_edge(int from, int to); // reason default set to (-1, -1)
  bool add_edge(int from, int to, std::pair<int, int> reason); // add (from, to, label)
  void remove_edge(int from, int to, std::pair<int, int> reason); // remove (from, to, label), assume (from, to, label) in the graph
  void get_minimal_cycle(std::vector<Lit> &cur_conflict_clauses); // get_minimal_cycle will copy conflict_clause and clear it
  void get_propagated_lits(std::vector<std::pair<Lit, std::vector<Lit>>> &cur_propagated_lit); // get_propagated_lits will copy propagated_lits and clear it 
  void set_var_status(int var, bool is_unassigned); 
  // * note: this is a bad implementation, for ICDGraph's original responsibility prevent itself from seeing these info.
};
}

#endif // MINISAT_ICDGRAPH_H