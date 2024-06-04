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
  added_edges_of.assign(polygraph->n_vars, {});
  known_induced_edges_of.assign(polygraph->n_vars, {});
  
  for (const auto &[from, to, type] : polygraph->known_edges) {
    icd_graph.add_known_edge(from, to /*, reason = {-1, -1} */); 
    if (type == 1) { // WW
      assert(polygraph->has_ww_keys(from, to));
      const auto &keys = polygraph->ww_keys[from][to];
      ww_keys[from][to].insert(keys.begin(), keys.end());
      for (const auto &key : keys) {
        ww_to[from][key].insert(to);
      } 
    } else if (type == 2) { // WR
      assert(polygraph->has_wr_keys(from, to));
      for (const auto &key : polygraph->wr_keys[from][to]) {
        wr_to[from][key].insert(to);
      }
    }
  }
  for (int v = 0; v < polygraph->n_vars; v++) { // move ww keys (in constraints) into a unified position
    if (polygraph->is_ww_var(v)) {
      const auto &[from, to, keys] = polygraph->ww_info[v];
      ww_keys[from][to].insert(keys.begin(), keys.end());
    }
  }
  
  {
    // initialize induced known edges
    Logger::log("[Initializing Induced Known Edges]");
    #ifdef INDUCE_KNOWN_EDGES
      Logger::log("- initailize all edges!");
    #endif

    using ReasonEdge = std::tuple<int, int, std::pair<int, int>>; // (from, to, reason)
    struct CmpReasonEdge {
      auto operator() (const ReasonEdge &lhs, const ReasonEdge &rhs) const -> bool {
        return lhs < rhs;
      };
    };
    // known_induced_edges_set will ensures duplicate-free reasonedges
    using known_induced_edges_set = std::set<ReasonEdge, CmpReasonEdge>;
    auto known_induced_edges_set_of = std::vector<known_induced_edges_set>(polygraph->n_vars);

    for (int v = 0; v < polygraph->n_vars; v++) {
      Logger::log(fmt::format("var {}:", v));
      auto &s = known_induced_edges_set_of[v];
      if (polygraph->is_ww_var(v)) {
        const auto &[from, to, keys] = polygraph->ww_info[v];
        s.insert(ReasonEdge{from, to, {v, -1}});
        Logger::log(fmt::format(" - WW: {} -> {}, reason = ({}, {}), Known(Itself)", from, to, v, -1));
      } else if (polygraph->is_wr_var(v)) {
        const auto &[from, to, key] = polygraph->wr_info[v];
        s.insert(ReasonEdge{from, to, {-1, v}});
        Logger::log(fmt::format(" - WR: {} -> {}, reason = ({}, {}), Known(Itself)", from, to, -1, v));
      } else {
        assert(false);
      }

      #ifdef INDUCE_KNOWN_EDGES
        // copied from add_edges_of_var()
        const auto &var = v;
        if (polygraph->is_ww_var(v)) {
          const auto &[from, to, keys] = polygraph->ww_info[var];
          for (const auto &key : keys) {
            if (wr_to[from][key].empty()) continue;
            for (const auto &to2 : wr_to[from][key]) {
              if (to2 == to) continue;
              auto var2 = polygraph->wr_var_of[from][to2][key];
              Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {}), Induced", to2, to, var, var2));
              s.insert(ReasonEdge{to2, to, {var, var2}});
            }
          }
        } else if (polygraph->is_wr_var(v)) {
          const auto &[from, to, key] = polygraph->wr_info[var];
          for (const auto &to2 : ww_to[from][key]) {
            if (to2 == to) continue;
            auto var2 = polygraph->ww_var_of[from][to2];
            Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {}), Induced", to, to2, var2, var));
            s.insert(ReasonEdge{to, to2, {var2, var}});
          }
        } else {
          assert(false);
        }
      #endif
    }

    // copy results into known_induced_edges
    for (int v = 0; v < polygraph->n_vars; v++) {
      const auto &edges_set = known_induced_edges_set_of[v];
      auto &edges = known_induced_edges_of[v];
      for (const auto &e :edges_set) {
        edges.emplace_back(e);
      } 
    }

    #ifdef INDUCE_KNOWN_EDGES
      // clear known edges
      ww_to.assign(polygraph->n_vertices, {});
      wr_to.assign(polygraph->n_vertices, {});
    #endif
  }

  // * note: assume known graph has reached a fixed point, 
  // * i.e. no more RW edge can be induced here
  // TODO: check acyclicity of known graph and report possible conflict

  // ! deprecated
  // initialize vars_heap, sorting by n_edges_of_var
  int n_vars = polygraph->n_vars;
  for (int i = 0; i < n_vars; i++) vars_heap.insert({known_induced_edges_of[i].size(), i});
}

void AcyclicSolverHelper::add_var(int var) {
  // TODO: test add_var()
  // add_var adds the var into vars_heap
  int n_edges = known_induced_edges_of[var].size();
  vars_heap.insert({n_edges, var});
  icd_graph.set_var_assigned(var, false);
} 

void AcyclicSolverHelper::remove_var(int var) {
  // TODO: test remove_var()
  // remove_var removes the var from vars_heap
  int n_edges = known_induced_edges_of[var].size();
  if (vars_heap.contains({n_edges, var})) {
    vars_heap.erase({n_edges, var});
    icd_graph.set_var_assigned(var, true);
  }
}

bool AcyclicSolverHelper::add_edges_of_var(int var) { 
  // return true if edge is successfully added into the graph, i.e. no cycle is detected 
  auto &added_edges = added_edges_of[var];
  assert(added_edges.empty());

  bool cycle = false;
  if (polygraph->is_ww_var(var)) {
    Logger::log(fmt::format("- adding {}, type = WW", var));
    // 1. add induced known edges, including itself(ww)
    // if INDUCE_KNOWN_EDGES is enabled, known_induced_known_edges_of[var] contains all induced edges(including itself)
    // otherwise, it will only contain itself(ww)
    const auto &known_induced_edges = known_induced_edges_of[var];
    for (const auto &[from, to, reason] : known_induced_edges) {
      Logger::log(fmt::format(" - ??: {} -> {}, reason = ({}, {}), Known", from, to, reason.first, reason.second));
      cycle = !icd_graph.add_edge(from, to, reason);
      if (cycle) {
        Logger::log(" - conflict!");
        goto conflict; // bad implementation
      } 
      Logger::log(" - success");
      added_edges.push({from, to, reason});
    }
    
    // 2. add induced rw edges
    const auto &[from, to, keys] = polygraph->ww_info[var];
    for (const auto &key : keys) {
      if (wr_to[from][key].empty()) continue;
      for (const auto &to2 : wr_to[from][key]) {
        if (to2 == to) continue;
        auto var2 = polygraph->wr_var_of[from][to2][key];
        Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {}), Induced", to2, to, var, var2));
        cycle = !icd_graph.add_edge(to2, to, {var, var2});
        if (cycle) {
          Logger::log(" - conflict!");
          goto conflict; // bad implementation
        } 
        Logger::log(" - success");
        added_edges.push({to2, to, {var, var2}});
      }
    }

    if (!cycle) {
      for (const auto &key : keys) {
        ww_to[from][key].insert(to);
        Logger::log(fmt::format(" - inserting ({} -> {}, key = {}) into ww_to, now ww_to[{}][{}] = {} ", from, to, key, from, key, Logger::urdset2str(ww_to[from][key])));
      }
    } 
  } else if (polygraph->is_wr_var(var)) {
    Logger::log(fmt::format("- adding {}, type = WR", var));

    // 1. add known induced edges, including itself(wr)
    // see ww branch for more info
    const auto &known_induced_edges = known_induced_edges_of[var];
    for (const auto &[from, to, reason] : known_induced_edges) {
      Logger::log(fmt::format(" - ??: {} -> {}, reason = ({}, {}), Known", from, to, reason.first, reason.second));
      cycle = !icd_graph.add_edge(from, to, reason);
      if (cycle) {
        Logger::log(" - conflict!");
        goto conflict; // bad implementation
      } 
      Logger::log(" - success");
      added_edges.push({from, to, reason});
    }

    // 2. add induced rw edges
    const auto &[from, to, key] = polygraph->wr_info[var];
    for (const auto &to2 : ww_to[from][key]) {
      if (to2 == to) continue;
      auto var2 = polygraph->ww_var_of[from][to2];
      Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {}), Induced", to, to2, var2, var));
      cycle = !icd_graph.add_edge(to, to2, {var2, var});
      if (cycle) {
        Logger::log(" - conflict!");
        break;
      } 
      Logger::log(" - success");
      added_edges.push({to, to2, {var2, var}});
    }

    if (!cycle) {
      wr_to[from][key].insert(to);
      Logger::log(fmt::format(" - inserting ({} -> {}, key = {}) into wr_to, now wr_to[{}][{}] = {}", from, to, key, from, key, Logger::urdset2str(wr_to[from][key])));
    } 
  } else {
    assert(false);
  }

  if (!cycle) {
    Logger::log(fmt::format(" - {} is successfully added", var));
    icd_graph.get_propagated_lits(propagated_lits);
    construct_wr_cons_propagated_lits(var);
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
    assert(added_edges.empty());
    return false;
} 

void AcyclicSolverHelper::remove_edges_of_var(int var) {
#ifndef ADDED_EDGES_CACHE
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
  while (!added_edges_of[var].empty()) add_edges_of_var[var].pop();

#else

  Logger::log(fmt::format("- removing {}, type = {}", var, (polygraph->is_ww_var(var) ? "WW" : "WR")));

  auto &added_edges = added_edges_of[var];
  while (!added_edges.empty()) {
    const auto &[from, to, reason] = added_edges.top();
    icd_graph.remove_edge(from, to, reason);
    Logger::log(fmt::format(" - ??: {} -> {}, reason = ({}, {})", from, to, reason.first, reason.second));
    added_edges.pop();
  }
  // all edges have been deleted including the WW or WR itself
  if (polygraph->is_ww_var(var)) {
    const auto &[from, to, keys] = polygraph->ww_info[var];
    for (const auto &key : keys) {
      assert(ww_to[from][key].contains(to));
      ww_to[from][key].erase(to);
      Logger::log(fmt::format(" - deleting ({} -> {}, key = {}) into ww_to, now ww_to[{}][{}] = {} ", from, to, key, from, key, Logger::urdset2str(ww_to[from][key])));
    }
  } else if (polygraph->is_wr_var(var)) { 
    const auto &[from, to, key] = polygraph->wr_info[var];
    assert(wr_to[from][key].contains(to));
    wr_to[from][key].erase(to);
    Logger::log(fmt::format(" - deleting ({} -> {}, key = {}) into wr_to, now wr_to[{}][{}] = {}", from, to, key, from, key, Logger::urdset2str(wr_to[from][key])));
  } else {
    assert(false);
  }

#endif

  assert(added_edges_of[var].empty());
}

Var AcyclicSolverHelper::get_var_represents_max_edges() {
  if (vars_heap.empty()) return var_Undef;
  auto it = --vars_heap.end();
  return Var(it->second);
}

Var AcyclicSolverHelper::get_var_represents_min_edges() {
  if (vars_heap.empty()) return var_Undef;
  auto it = vars_heap.begin();
  return Var(it->second);
}

void AcyclicSolverHelper::construct_wr_cons_propagated_lits(int var) {
  // construct wr constraint propagated lits
#ifdef ENABLE_WRCP
  if (!polygraph->is_wr_var(var)) return;
  Logger::log(fmt::format("- [Construct WRCP of {}]", var));
  auto wr_cons_ref = polygraph->get_wr_cons(var);
  if (!wr_cons_ref) {
    Logger::log(fmt::format("- unit wr cons {}, end WRCP", var));
    return;
  }
  for (const auto &var2 : *wr_cons_ref) {
    if (icd_graph.get_var_assigned(var2)) continue;
    Logger::log(fmt::format(" - prop (~{}) with reason (~{} | ~{})", var2, var, var2));
    propagated_lits.emplace_back(std::pair<Lit, std::vector<Lit>>{~mkLit(var2), {~mkLit(var), ~mkLit(var2)}});
  }
  Logger::log(fmt::format("- end of WRCP construction", var));
#endif
}

Polygraph *AcyclicSolverHelper::get_polygraph() { return polygraph; }

namespace Logger {

// ! This is a VERY BAD implementation, see Logger.h for more details
auto wr_to2str(const std::unordered_set<std::pair<int, int64_t>, decltype(pair_hash_endpoint)> &s) -> std::string {
  if (s.empty()) return std::string{"null"};
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