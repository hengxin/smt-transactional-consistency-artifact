#include <unordered_set>
#include <vector>
#include <tuple>
#include <cmath>
#include <iostream>
#include <queue>
#include <algorithm>
#include <random>
#include <chrono>
#include <unordered_map>

#include "ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"
#include "OptOption.h"
#include "minisat/utils/Monitor.h"

namespace Minisat {

ICDGraph::ICDGraph() {}

void ICDGraph::init(int _n_vertices = 0, int n_vars = 0) {
  n = _n_vertices, m = max_m = 0;
  level.assign(n, 1);
  in.assign(n, std::unordered_set<int>());
  out.assign(n, std::unordered_set<int>());
}

bool ICDGraph::construct_backward_cycle(std::vector<int> &backward_pred, int from, int to, int ww_reason, int rw_reason) {
  // always return true
  std::vector<int> vars;
  auto add_var = [&vars](const std::pair<int, int> reason) -> bool {
    bool added = false;
    const auto &[r1, r2] = reason;
    if (r1 != -1) added = true, vars.emplace_back(r1);
    if (r2 != -1) added = true, vars.emplace_back(r2);
    return added;
  };

  for (int x = to; x != from; ) {
    const auto &pred = backward_pred[x];
    assert(!edge_reasons[x][pred].empty());
    add_var(*edge_reasons[x][pred].begin());
    x = pred;
  }
  add_var(std::make_pair(ww_reason, rw_reason));
  std::sort(vars.begin(), vars.end());
  vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
  conflict_clause.clear();
  for (auto v : vars) conflict_clause.emplace_back(mkLit(v, false));
  return true;
}

bool ICDGraph::construct_forward_cycle(std::vector<int> &backward_pred, 
                               std::vector<int> &forward_pred, 
                               int from, int to, int ww_reason, int rw_reason, int middle) {
  // always return true
  std::vector<int> vars;
  auto add_var = [&vars](const std::pair<int, int> reason) -> bool {
    bool added = false;
    const auto &[r1, r2] = reason;
    if (r1 != -1) added = true, vars.emplace_back(r1);
    if (r2 != -1) added = true, vars.emplace_back(r2);
    return added;
  };
  for (int x = middle; x != to; ) {
    const auto &pred = forward_pred[x];
    assert(!edge_reasons[pred][x].empty());
    add_var(*edge_reasons[pred][x].begin());
    x = pred;
  }
  for (int x = middle; x != from; ) {
    const auto &pred = backward_pred[x];
    assert(!edge_reasons[x][pred].empty());
    add_var(*edge_reasons[x][pred].begin()); 
    x = pred;
  }
  add_var(std::make_pair(ww_reason, rw_reason));
  std::sort(vars.begin(), vars.end());
  vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
  conflict_clause.clear();
  for (auto v : vars) conflict_clause.emplace_back(mkLit(v, false));
  return true;
}

bool ICDGraph::detect_cycle(int from, int to, int ww_reason, int rw_reason) {
  if (!conflict_clause.empty()) {
    std::cerr << "Oops! conflict_clause is not empty when entering detect_cycle()" << std::endl;
    conflict_clause.clear();
  }
  if (level[from] < level[to]) return false;

  // step 1. search backward
  std::vector<int> backward_pred(n);
  std::unordered_set<int> backward_visited;
  std::queue<int> q;
  q.push(from), backward_visited.insert(from);
  int delta = (int)std::min(std::pow(n, 2.0 / 3), std::pow(max_m, 1.0 / 2)) / 10 + 1, visited_cnt = 0;
  // int delta = (int)(std::pow(max_m, 1.0 / 2)) / 8 + 1, visited_cnt = 0;
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (x == to) return construct_backward_cycle(backward_pred, from, to, ww_reason, rw_reason);
    ++visited_cnt;
    if (visited_cnt >= delta) break;
    for (auto &y : in[x]) {
      if (backward_visited.contains(y)) continue;
      backward_visited.insert(y);
      backward_pred[y] = x;
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
  std::vector<int> forward_pred(n);
  std::unordered_set<int> forward_visited;
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (forward_visited.contains(x)) continue;
    for (auto &y : out[x]) {
      if (backward_visited.contains(y)) {
        forward_pred[y] = x;
        return construct_forward_cycle(backward_pred, forward_pred, from, to, ww_reason, rw_reason, y);
      }
      if (level[x] == level[y]) {
        in[y].insert(x);
      } else if (level[y] < level[x]) {
        level[y] = level[x];
        in[y].clear();
        in[y].insert(x);
        forward_pred[y] = x;
        q.push(y);
      }
    }
    forward_visited.insert(x);
  }
  return false;
}

bool ICDGraph::add_known_edge(int from, int to, int dep_reason, int antidep_reason) {
  // add_known_edge doesn't check acyclicity
  auto &reasons = edge_reasons[from][to];
  if (!reasons.empty()) {
    reasons.insert(std::make_pair(dep_reason, antidep_reason));
    return true;
  }
  out[from].insert(to);
  if (level[from] == level[to]) in[to].insert(from);
  if (++m > max_m) max_m = m;
  reasons.insert(std::make_pair(dep_reason, antidep_reason));
  return true; // always return true
}

bool ICDGraph::add_edge(int from, int to, int ww_reason, int rw_reason) {
  auto &reasons = edge_reasons[from][to];
  if (!reasons.empty()) {
    reasons.insert(std::make_pair(ww_reason, rw_reason));
    return true;
  }
  if (!detect_cycle(from, to, ww_reason, rw_reason)) {
    out[from].insert(to);
    if (level[from] == level[to]) in[to].insert(from);
    if (++m > max_m) max_m = m;
    reasons.insert(std::make_pair(ww_reason, rw_reason));
    return true;
  }
  return false;
}

void ICDGraph::remove_edge(int from, int to, int ww_reason, int rw_reason) {
  auto &reasons = edge_reasons[from][to];
  if (!reasons.contains(std::make_pair(ww_reason, rw_reason))) return;
  reasons.erase(reasons.find(std::make_pair(ww_reason, rw_reason)));
  if (reasons.empty()) { // reasons contains exactly 1 reason, delete this edge of (in & out)
    if (out[from].contains(to)) out[from].erase(to);
    if (in[to].contains(from)) in[to].erase(from);
    --m;
  }
}

void ICDGraph::get_minimal_cycle(std::vector<Lit> &cur_conflict_clauses) {
  cur_conflict_clauses.clear();
  for (auto lit : conflict_clause) cur_conflict_clauses.push_back(lit);
  conflict_clause.clear();
}

} // namespace Minisat