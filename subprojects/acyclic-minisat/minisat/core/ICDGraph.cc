#include <unordered_set>
#include <vector>
#include <tuple>
#include <cmath>
#include <iostream>
#include <queue>
#include <algorithm>
#include <unordered_map>

#include "ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"
#include "OptOption.h"

namespace Minisat {

using EdgeInfo = std::pair<int, int>;
using ICDGraphEdge = std::tuple<int, int, int>;

ICDGraph::ICDGraph() {}
void ICDGraph::init(int _n_vertices = 0) {
  n = _n_vertices, m = max_m = 0;
  level.assign(n, 1);
  in.assign(n, std::unordered_set<EdgeInfo, decltype(edge_info_hash_endpoint)>());
  out.assign(n, std::unordered_set<EdgeInfo, decltype(edge_info_hash_endpoint)>());
  inactive_edges.assign(n, std::unordered_set<EdgeInfo, decltype(edge_info_hash_endpoint)>());
}

void ICDGraph::add_inactive_edge(int from, int to, int label) { inactive_edges[from].insert(std::make_pair(to, label)); }

bool ICDGraph::add_edge(int from, int to, int label, bool need_detect_cycle) { 
  // if a cycle is detected, the edge will not be added into the graph
  if (!need_detect_cycle || !detect_cycle(from, to, label)) {
    out[from].insert(std::make_pair(to, label));
    if (level[from] == level[to]) {
      in[to].insert(std::make_pair(from, label));
    }
    ++m;
    if (m > max_m) max_m = m;
    if (inactive_edges[from].contains(std::make_pair(to, label))) {
      inactive_edges[from].erase(std::make_pair(to, label));
    }
    return true;
  }
  return false;
}

void ICDGraph::remove_edge(int from, int to, int label) {
  if (out[from].contains(std::make_pair(to, label))) {
    out[from].erase(std::make_pair(to, label));
  }
  if (in[to].contains(std::make_pair(from, label))) {
    in[to].erase(std::make_pair(from, label));
  }
  --m;
  inactive_edges[from].insert(std::make_pair(to, label));
}

void ICDGraph::get_minimal_cycle(std::vector<Lit> &cur_conflict_clauses) {
  cur_conflict_clauses.clear();
  for (auto lit : conflict_clause) cur_conflict_clauses.push_back(lit);
  conflict_clause.clear();
}

void ICDGraph::get_propagated_lits(std::vector<Lit> &cur_propagated_lits) {
  cur_propagated_lits.clear();
  for (auto lit : propagated_lits) cur_propagated_lits.push_back(lit);
  propagated_lits.clear();
}

bool ICDGraph::detect_cycle(int from, int to, int label) {
  // if any cycle is detected, construct conflict_clause and return true, else (may skip)construct propagated_lits and return false
  if (!conflict_clause.empty()) {
    std::cerr << "Oops! conflict_clause is not empty when entering detect_cycle()" << std::endl;
    conflict_clause.clear();
  }
  if (level[from] < level[to]) return false;

  // step 1. search backward
  std::vector<EdgeInfo> backward_pred(n);
  std::unordered_set<int> backward_visited;
  std::queue<int> q;
  q.push(from), backward_visited.insert(from);
  bool cycle = false;
  // int delta = (int)std::min(std::pow(n, 2.0 / 3), std::pow(max_m, 1.0 / 2)) / 8 + 1, visited_cnt = 0;
  int delta = (int)(std::pow(max_m, 1.0 / 2)) / 8 + 1, visited_cnt = 0;
  // TODO: count current n_vertices and n_edges dynamically
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (x == to) return construct_backward_cycle(backward_pred, from, to, label);
    ++visited_cnt;
    if (visited_cnt >= delta) break;
    for (auto &[y, l] : in[x]) {
      if (backward_visited.contains(y)) continue;
      backward_visited.insert(y);
      backward_pred[y] = std::make_pair(x, l);
      q.push(y);
    }
  }
  if (visited_cnt < delta) {
    if (level[from] == level[to]) return false;
    if (level[from] > level[to]) { // must be true
      level[to] = level[from];
    }
  } else { // traverse at least delta arcs
    level[to] = level[from] + 1;
    backward_visited.clear();
    backward_visited.insert(from);
  }
  in[to].clear();

  // step 2. search forward 
  while (!q.empty()) q.pop();
  q.push(to);
  std::vector<EdgeInfo> forward_pred(n);
  std::unordered_set<int> forward_visited;
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (forward_visited.contains(x)) continue;
    for (auto &[y, l] : out[x]) {
      if (backward_visited.contains(y)) {
        forward_pred[y] = std::make_pair(x, l);
        return construct_forward_cycle(backward_pred, forward_pred, from, to, label, y);
      }
      if (level[x] == level[y]) {
        in[y].insert(std::make_pair(x, l));
      } else if (level[y] < level[x]) {
        level[y] = level[x];
        in[y].clear();
        in[y].insert(std::make_pair(x, l));
        forward_pred[y] = std::make_pair(x, l);
        q.push(y);
      }
    }
    forward_visited.insert(x);
  }
  construct_propagated_lits(forward_visited, backward_visited);
  return false;
}

void ICDGraph::construct_propagated_lits(std::unordered_set<int> &forward_visited, std::unordered_set<int> &backward_visited) {
  propagated_lits.clear();

#ifdef EXTEND_VERTICES_IN_UEP
  const int MAX_N_EXTEND_VERTICES = 55;
  int count = 0;
  std::queue<int> q;
  for (auto x : forward_visited) q.push(x);
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (++count >= MAX_N_EXTEND_VERTICES) break;
    for (const auto &[y, _] : out[x]) {
      if (forward_visited.contains(y)) continue;
      q.push(y);
      forward_visited.insert(y);
    }
  }
  count = 0;
  while (!q.empty()) q.pop();
  for (auto x : backward_visited) q.push(x);
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (++count >= MAX_N_EXTEND_VERTICES) break;
    for (const auto &[y, _] : in[x]) {
      if (backward_visited.contains(y)) continue;
      q.push(y);
      backward_visited.insert(y);
    }
  }
#endif


  for (int x : forward_visited) {
    for (const auto &[y, label] : inactive_edges[x]) {
      if (backward_visited.contains(y)) {
        propagated_lits.push_back(mkLit((Var)label, false)); // sign = false: v, sign = true: ~v
      }
    }
  }
}

bool ICDGraph::construct_backward_cycle(std::vector<EdgeInfo> &backward_pred, int from, int to, int label) {
  // always return true
  std::vector<int> vars;
  for (int x = to; x != from; ) {
    const auto &[pred, l] = backward_pred[x];
    if (l != -1) vars.push_back(l);
    x = pred;
  }
  if (label != -1) vars.push_back(label);
  std::sort(vars.begin(), vars.end());
  vars.erase(unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.push_back(mkLit(v, false));
  }
  return true;
}

bool ICDGraph::construct_forward_cycle(std::vector<EdgeInfo> &backward_pred,
                                       std::vector<EdgeInfo> &forward_pred,
                                       int from, int to, int label, int middle) {
  // always return true
  std::vector<int> vars;
  for (int x = middle; x != to; ) {
    const auto &[pred, l] = forward_pred[x];
    if (l != -1) vars.push_back(l);
    x = pred;
  }
  for (int x = middle; x != from; ) {
    const auto &[pred, l] = backward_pred[x];
    if (l != -1) vars.push_back(l);
    x = pred;
  }
  if (label != -1) vars.push_back(label);
  std::sort(vars.begin(), vars.end());
  vars.erase(unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.push_back(mkLit(v, false));
  } 
  return true;
}

} // namespace Minisat