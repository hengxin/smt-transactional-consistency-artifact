#include <cassert>
#include <algorithm>
#include <queue>
#include <iostream>

#include "ReachGraph.h"

namespace Minisat {

ReachGraph::ReachGraph() {}

void ReachGraph::init(int _n) {
  n = _n, m = unstacked_edges_cnt = 0;
  edges.assign(n, std::unordered_multiset<int>());
}

void ReachGraph::init_reachability() { push_reachability(); }

void ReachGraph::add_edge(int from, int to, bool need_count) {
  edges[from].insert(to);
  if (need_count) ++m;
  // if (m >= REBUILD_THRESHOLD * int(reachability_trail.size())) push_reachability();
}

void ReachGraph::remove_edge(int from, int to) {
  if (edges[from].contains(to)) {
    edges[from].erase(edges[from].find(to));
    --m;
    // if (m < REBUILD_THRESHOLD * (int(reachability_trail.size()) - 1)) pop_reachability();
  }
}

void ReachGraph::push_reachability() {
  auto reachability = [&]() -> Reachability {
    Reachability reachability(n);

    auto reversed_topo_order = [&]() {
      std::vector<int> order;
      std::vector<int> deg(n, 0);
      for (int x = 0; x < n; x++) {
        for (int y : edges[x]) {
          ++deg[y];
        }
      }
      std::queue<int> q;
      for (int x = 0; x < n; x++) {
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
      // std::cerr << order.size() << " " << n << std::endl;
      assert(order.size() == n);
      std::reverse(order.begin(), order.end());
      return std::move(order);
    }();

    for (int x : reversed_topo_order) {
      reachability.at(x).set(x);
      for (int y : edges[x]) {
        reachability.at(x) |= reachability.at(y);
      }
    }

    return std::move(reachability);
  }();

  reachability_trail.push_back(reachability);
}

void ReachGraph::pop_reachability() {
  assert(reachability_trail.size() > 1);
  reachability_trail.pop_back();
}

bool ReachGraph::can_reach(int from, int to) { return reachability_trail.back().at(from).test(to); }

} // namespace Minisat