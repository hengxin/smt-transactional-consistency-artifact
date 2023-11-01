#ifndef MINISAT_REACH_GRAPH_H
#define MINISAT_REACH_GRAPH_H

#include <unordered_set>
#include <vector>
#include <utility>
#include <bitset>

namespace Minisat {

const int MAX_N = 20000;
const int REBUILD_THRESHOLD = 5000;

class ReachGraph {
  using Reachability = std::vector<std::bitset<MAX_N>>;

  int n, m, unstacked_edges_cnt;
  std::vector<std::unordered_multiset<int>> edges;
  std::vector<Reachability> reachability_trail;

  void push_reachability();
  void pop_reachability();

public:
  ReachGraph();
  void init(int _n);
  void init_reachability(); // call once, exactly after adding all edges of known graph
  void add_edge(int from, int to, bool need_count = true);
  void remove_edge(int from, int to);
  bool can_reach(int from, int to);

}; // class ReachGraph 

} // namespace Minisat

#endif // MINISAT_REACH_GRAPH_H