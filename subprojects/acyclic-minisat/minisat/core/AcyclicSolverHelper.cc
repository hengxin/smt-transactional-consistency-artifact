// FIXME: see google cpp standard to reorder head files

#include "AcyclicSolverHelper.h" 

#include <iostream>
#include <vector>
#include <bitset>
#include <tuple>
#include <stack>
#include <cassert>
#include <fmt/format.h>

#include "minisat/core/Polygraph.h"
#include "minisat/core/ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"
#include "minisat/core/OptOption.h"
#include "minisat/core/Logger.h"

namespace Minisat {

AcyclicSolverHelper::AcyclicSolverHelper(Polygraph *_polygraph) {
  polygraph = _polygraph;
  icd_graph.init(polygraph->n_vertices, polygraph->n_vars);
  conflict_clauses.clear();
  ww_to.assign(polygraph->n_vertices, {});
  wr_to.assign(polygraph->n_vertices, {});
  
  for (const auto &[from, to, type] : polygraph->known_edges) {
    icd_graph.add_known_edge(from, to /*, reason = {-1, -1} */); 
    if (type == 1) { // WW
      ww_to[from].insert(to);
      assert(polygraph->has_ww_keys(from, to));
      const auto &keys = polygraph->ww_keys[from][to];
      ww_keys[from][to].insert(keys.begin(), keys.end());
    } else if (type == 2) { // WR
      assert(polygraph->has_wr_keys(from, to));
      for (const auto &key : polygraph->wr_keys[from][to]) {
        wr_to[from].insert({to, key});
      }
    }
  }
  for (int v = 0; v < polygraph->n_vars; v++) { // move ww keys (in constraints) into a unified position
    if (polygraph->is_ww_var(v)) {
      const auto &[from, to, keys] = polygraph->ww_info[v];
      ww_keys[from][to].insert(keys.begin(), keys.end());
    }
  }

  // * note: assume known graph has reached a fixed point, 
  // * i.e. no more RW edge can be induced here
  // TODO: check acyclicity of known graph and report possible conflict

  // ! deprecated
  // initialize vars_heap, sorting by n_edges_of_var
  int n_vars = polygraph->n_vars;
  for (int i = 0; i < n_vars; i++) vars_heap.insert(std::make_pair(/* edge(s) of each var = */ 1, i));

#ifdef EXTEND_KG_IN_UEP
  // ! deprecated
  if (polygraph->n_vertices > MAX_N) {
    std::cerr << "skip extending known graph in unit edge propagation for n = " 
              << polygraph->n_vertices << " > MAX_N" << std::endl;
    return;
  }
  auto reachability = [&](bool reverse = false) -> std::vector<std::bitset<MAX_N>> {
    int n = polygraph->n_vertices;
    std::vector<std::vector<int>> g(n);

    for (const auto &[from, to] : polygraph->known_edges) {
      if (reverse) g[to].push_back(from);
      else g[from].push_back(to);
    }
    
    auto reversed_topo_order = [&]() {
      std::vector<int> order;
      std::vector<int> deg(n, 0);
      for (int x = 0; x < n; x++) {
        for (int y : g[x]) {
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
        for (int y : g[x]) {
          --deg[y];
          if (!deg[y]) q.push(y);
        }
      }
      assert(int(order.size()) == n);
      std::reverse(order.begin(), order.end());
      return std::move(order);
    }();
    
    auto reachability = std::vector<std::bitset<MAX_N>>(n, std::bitset<MAX_N>());
    for (int x : reversed_topo_order) {
      reachability.at(x).set(x);
      for (int y : g[x]) {
        reachability.at(x) |= reachability.at(y);
      }
    }

    return std::move(reachability);
  };
  auto known_graph_reach_to = reachability();
  auto known_graph_reach_from = reachability(true);

  for (int v = 0; v < n_vars; v++) {
    using ReachSet = std::bitset<MAX_N>;
    auto reach_from = ReachSet{}, reach_to = ReachSet{};
    for (const auto &[from, to] : polygraph->edges.at(v)) {
      reach_from |= known_graph_reach_from.at(from);
      reach_to |= known_graph_reach_to.at(to);
    }
    icd_graph.push_into_var_reachsets(reach_from, reach_to);
    
    // std::cerr << reach_from << " " << reach_to << std::endl;
  }

#endif

}

void AcyclicSolverHelper::add_var(int var) {
  // TODO later: add_var
  // now pass
} 

void AcyclicSolverHelper::remove_var(int var) {
  // TODO later: remove_var
  // now pass
}

bool AcyclicSolverHelper::add_edges_of_var(int var) { 
  // return true if edge is successfully added into the graph, i.e. no cycle is detected 
  // TODO: test add edge(s) of var
  std::stack<std::tuple<int, int, std::pair<int, int>>> added_edges;
  bool cycle = false;

  if (polygraph->is_ww_var(var)) {
    Logger::log(fmt::format("- adding {}, type = WW", var));
    const auto &[from, to, keys] = polygraph->ww_info[var];
    // 1. add itself, ww
    Logger::log(fmt::format(" - WW: {} -> {}, reason = ({}, {})", from, to, var, -1));
    cycle = !icd_graph.add_edge(from, to, {var, -1});
    if (cycle) {
      Logger::log(" - conflict!");
      goto conflict; // bad implementation
    } 
    Logger::log(" - success");
    added_edges.push({from, to, {var, -1}});
    
    // 2. add induced rw edges
    for (const auto &[to2, key2] : wr_to[from]) {
      if (to2 == to) continue;
      if (!keys.contains(key2)) continue;
      // RW: to2 -> to
      auto var2 = polygraph->wr_var_of[from][to2][key2];
      Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {})", to2, to, var, var2));
      cycle = !icd_graph.add_edge(to2, to, {var, var2});
      if (cycle) {
        Logger::log(" - conflict!");
        break;
      } 
      Logger::log(" - success");
      added_edges.push({to2, to, {var, var2}});
    }
    if (!cycle) {
      ww_to[from].insert(to);
      Logger::log(fmt::format(" - inserting {} -> {} into ww_to, now ww_to[{}] = {} ", from, to, from, Logger::urdset2str(ww_to[from])));
    } 
  } else if (polygraph->is_wr_var(var)) {
    Logger::log(fmt::format("- adding {}, type = WR", var));
    const auto &[from, to, key] = polygraph->wr_info[var];
    // 1. add itself, wr
    Logger::log(fmt::format(" - WR: {} -> {}, reason = ({}, {})", from, to, -1, var));
    cycle = !icd_graph.add_edge(from, to, {-1, var});
    if (cycle) {
      Logger::log(" - conflict!");
      goto conflict; // bad implementation
    } 
    Logger::log(" - success");
    added_edges.push({from, to, {-1, var}});

    // 2. add induced rw edges
    for (const auto &to2 : ww_to[from]) {
      if (to2 == to) continue;
      const auto &key2s = ww_keys[from][to2];
      if (!key2s.contains(key)) continue;
      // RW: to -> to2
      auto var2 = polygraph->ww_var_of[from][to2];
      Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {})", to, to2, var2, var));
      cycle = !icd_graph.add_edge(to, to2, {var2, var});
      if (cycle) {
        Logger::log(" - conflict!");
        break;
      } 
      Logger::log(" - success");
      added_edges.push({to, to2, {var2, var}});
    }
    if (!cycle) {
      wr_to[from].insert({to, key});
      Logger::log(fmt::format(" - inserting ({} -> {}, key = {}) into wr_to, now wr_to[{}] = {}", from, to, key, from, Logger::wr_to2str(wr_to[from])));
    } 
  } else {
    assert(false);
  }

  if (!cycle) {
    Logger::log(fmt::format(" - {} is successfully added", var));
    return true;
  } 

  conflict:
    Logger::log(fmt::format(" - conflict found! reverting adding edges of {}", var));

    // generate conflict clause
    std::vector<Lit> cur_conflict_clause;
    icd_graph.get_minimal_cycle(cur_conflict_clause);

    // for (Lit l : cur_conflict_clause) std::cerr << l.x << " ";
    // std::cerr << std::endl;

    conflict_clauses.emplace_back(cur_conflict_clause);
    while (!added_edges.empty()) {
      const auto &[from, to, reason] = added_edges.top();
      Logger::log(fmt::format(" - removing edge {} -> {}, reason = ({}, {})", from, to, reason.first, reason.second));
      icd_graph.remove_edge(from, to, reason);
      added_edges.pop();
    }
    Logger::log(fmt::format(" - {} is not been added", var));
    return false;
} 

void AcyclicSolverHelper::remove_edges_of_var(int var) {
  // TODO: test remove edge(s) of var
  if (polygraph->is_ww_var(var)) {
    Logger::log(fmt::format("- removing {}, type = WW", var));
    const auto &[from, to, keys] = polygraph->ww_info[var];
    // 1. remove itself, ww
    Logger::log(fmt::format(" - WW: {} -> {}, reason = ({}, {})", from, to, var, -1));
    icd_graph.remove_edge(from, to, {var, -1});
    // 2. remove induced rw edges
    for (const auto &[to2, key2] : wr_to[from]) {
      if (to2 == to) continue;
      if (!keys.contains(key2)) continue;
      // RW: to2 -> to
      auto var2 = polygraph->wr_var_of[from][to2][key2];
      Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {})", to2, to, var, var2));
      icd_graph.remove_edge(to2, to, {var, var2});
    }
    assert(ww_to[from].contains(to));
    ww_to[from].erase(to);
    Logger::log(fmt::format(" - deleting {} -> {} into ww_to, now ww_to[{}] = {} ", from, to, from, Logger::urdset2str(ww_to[from])));
  } else if (polygraph->is_wr_var(var)) {
    Logger::log(fmt::format("- removing {}, type = WR", var));
    const auto &[from, to, key] = polygraph->wr_info[var];
    // 1. remove itself, wr
    Logger::log(fmt::format(" - WR: {} -> {}, reason = ({}, {})", from, to, -1, var));
    icd_graph.remove_edge(from, to, {-1, var});
    // 2. add induced rw edges
    for (const auto &to2 : ww_to[from]) {
      if (to2 == to) continue;
      const auto &key2s = ww_keys[from][to2];
      if (!key2s.contains(key)) continue;
      // RW: to -> to2
      auto var2 = polygraph->ww_var_of[from][to2];
      Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {})", to, to2, var2, var));
      icd_graph.remove_edge(to, to2, {var2, var});
    }
    assert(wr_to[from].contains({to, key}));
    wr_to[from].erase({to, key});
    Logger::log(fmt::format(" - deleting ({} -> {}, key = {}) into wr_to, now wr_to[{}] = {}", from, to, key, from, Logger::wr_to2str(wr_to[from])));
  } else {
    assert(false);
  }
}

Var AcyclicSolverHelper::get_var_represents_max_edges() {
  // ! deprecated
  if (vars_heap.empty()) return var_Undef;
  auto it = --vars_heap.end();
  return Var(it->second);
}

Var AcyclicSolverHelper::get_var_represents_min_edges() {
  // ! deprecated
  if (vars_heap.empty()) return var_Undef;
  auto it = vars_heap.begin();
  return Var(it->second);
}

namespace Logger {

// ! This is a VERY BAD implementation, see Logger.h for more details
auto wr_to2str(const std::unordered_set<std::pair<int, int64_t>, decltype(pair_hash_endpoint)> &s) -> std::string {
  if (s.empty()) return std::string{""};
  auto os = std::ostringstream{};
  for (const auto &[x, y] : s) {
    // std::cout << std::to_string(i) << std::endl;
    os << "{" << std::to_string(x) << ", " << std::to_string(y) << "}, ";
  } 
  auto ret = os.str();
  ret.erase(ret.length() - 2); // delete last ", "
  // std::cout << "\"" << ret << "\"" << std::endl;
  return ret; 
}

} // namespace Minisat::Logger

} // namespace Minisat