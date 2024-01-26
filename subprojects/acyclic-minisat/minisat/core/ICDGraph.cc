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
  // note: currently n_vars is useless
  n = _n_vertices, m = max_m = 0;
  level.assign(n, 1), in.assign(n, {}), out.assign(n, {});
  is_var_unassigned.assign(n_vars, true);
}

void ICDGraph::add_inactive_edge(int from, int to, std::pair<int, int> reason) { 
  // TODO later: now deprecated
}

bool ICDGraph::add_known_edge(int from, int to) { // reason default set to (-1, -1)
  // TODO: test add known edge
  // add_known_edge should not be called after initialisation
  if (!reasons_of[from][to].empty()) return true;
  reasons_of[from][to].insert({-1, -1});
  out[from].insert(to);
  if (level[from] == level[to]) in[to].insert(from);
  if (++m > max_m) max_m = m;
  return true; // always returns true
}

bool ICDGraph::add_edge(int from, int to, std::pair<int, int> reason) { 
  // if a cycle is detected, the edge will not be added into the graph
  // TODO: test add edge
  if (reasons_of.contains(from) && reasons_of[from].contains(to) && !reasons_of[from][to].empty()) {
    reasons_of[from][to].insert(reason);
    return true;
  }
  if (!detect_cycle(from, to, reason)) {
    reasons_of[from][to].insert(reason);
    out[from].insert(to);
    if (level[from] == level[to]) in[to].insert(from);
    if (++m > max_m) max_m = m;
    return true;
  }
  return false;
}

void ICDGraph::remove_edge(int from, int to, std::pair<int, int> reason) {
  // TODO: test remove edge
  assert(reasons_of.contains(from));
  assert(reasons_of[from].contains(to));
  auto &reasons = reasons_of[from][to];
  assert(reasons.contains(reason));
  reasons.erase(reason);
  if (reasons.empty()) {
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

void ICDGraph::get_propagated_lits(std::vector<std::pair<Lit, std::vector<Lit>>> &cur_propagated_lits) {
  cur_propagated_lits.clear();
  for (auto lit : propagated_lits) cur_propagated_lits.push_back(lit);
  propagated_lits.clear();
}

bool ICDGraph::detect_cycle(int from, int to, std::pair<int, int> reason) {
  // if any cycle is detected, construct conflict_clause and return true, else (may skip)construct propagated_lits and return false
  if (!conflict_clause.empty()) {
    std::cerr << "Oops! conflict_clause is not empty when entering detect_cycle()" << std::endl;
    conflict_clause.clear();
  }

  // TODO: test detect cycle
  if (level[from] < level[to]) return false;

  // step 1. search backward
  std::vector<int> backward_pred(n);
  std::unordered_set<int> backward_visited;
  std::queue<int> q;
  q.push(from), backward_visited.insert(from);
  int delta = (int)std::min(std::pow(n, 2.0 / 3), std::pow(max_m, 1.0 / 2)) / 10 + 1, visited_cnt = 0;
  // int delta = (int)(std::pow(max_m, 1.0 / 2)) / 8 + 1, visited_cnt = 0;
  // TODO: count current n_vertices and n_edges dynamically
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (x == to) return construct_backward_cycle(backward_pred, from, to, reason);
    ++visited_cnt;
    if (visited_cnt >= delta) break;
    for (const auto &y : in[x]) {
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
    for (const auto &y : out[x]) {
      if (backward_visited.contains(y)) {
        forward_pred[y] = x;
        return construct_forward_cycle(backward_pred, forward_pred, from, to, reason, y);
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
#ifdef ENABLE_UEP
  construct_propagated_lits(forward_visited, backward_visited, forward_pred, backward_pred, from, to, label);
#endif  
  return false;
}

void ICDGraph::construct_propagated_lits(std::unordered_set<int> &forward_visited, 
                                         std::unordered_set<int> &backward_visited,
                                         std::vector<int> &forward_pred,
                                         std::vector<int> &backward_pred,
                                         int from, int to, std::pair<int, int> reason) {
  // TODO later: construct propagated lits
  // now pass
}

bool ICDGraph::construct_backward_cycle(std::vector<int> &backward_pred, int from, int to, std::pair<int, int> reason) {
  // TODO: test construct backward cycle
  auto vars = std::vector<int>{};

  auto add_var = [&vars](int v) -> bool {
    if (v == -1) return false;
    vars.emplace_back(v);
    return true;
  };

  for (int x = to; x != from; ) {
    const int pred = backward_pred[x];
    assert(reasons_of.contains(x) && reasons_of[x].contains(pred) && !reasons_of[x][pred].empty());
    const auto [reason1, reason2] = *reasons_of[x][pred].begin();
    add_var(reason1), add_var(reason2);
  }

  {
    const auto [reason1, reason2] = reason;
    add_var(reason1), add_var(reason2);
  }

  std::sort(vars.begin(), vars.end());
  vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.emplace_back(mkLit(v));
  }
  return true;  // always returns true
}

bool ICDGraph::construct_forward_cycle(std::vector<int> &backward_pred, 
                                       std::vector<int> &forward_pred, 
                                       int from, int to, std::pair<int, int> reason, int middle) {
  // TODO: test construct forward cycle
  auto vars = std::vector<int>{};

  auto add_var = [&vars](int v) -> bool {
    if (!v) return false;
    vars.emplace_back(v);
    return true;
  };

  for (int x = middle; x != to; ) {
    const int pred = forward_pred[x];
    assert(reasons_of.contains(pred) && reasons_of[pred].contains(x) && !reasons_of[pred][x].empty());
    const auto [reason1, reason2] = *reasons_of[pred][x].begin();
    add_var(reason1), add_var(reason2);
  }

  for (int x = middle; x != from; ) {
    const int pred = backward_pred[x];
    assert(reasons_of.contains(x) && reasons_of[x].contains(pred) && !reasons_of[x][pred].empty());
    const auto [reason1, reason2] = *reasons_of[x][pred].begin();
    add_var(reason1), add_var(reason2);
  }

  {
    const auto [reason1, reason2] = reason;
    add_var(reason1), add_var(reason2);
  }


  std::sort(vars.begin(), vars.end());
  vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.emplace_back(mkLit(v));
  }
  return true; // always returns true
}

bool ICDGraph::check_acyclicity() {
  // TODO later: check acyclicity, toposort
  return true;
}

void ICDGraph::set_var_status(int var, bool is_unassgined) { is_var_unassigned[var] = is_unassgined; }

} // namespace Minisat