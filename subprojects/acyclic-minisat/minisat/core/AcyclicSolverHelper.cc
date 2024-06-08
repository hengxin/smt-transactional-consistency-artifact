// FIXME: see google cpp standard to reorder head files

#include "AcyclicSolverHelper.h" 

#include <iostream>
#include <vector>
#include <bitset>
#include <tuple>
#include <stack>
#include <queue>
#include <cassert>
#include <random>
#include <fmt/format.h>

#include "minisat/core/Polygraph.h"
#include "minisat/core/ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"
#include "minisat/core/OptOption.h"
#include "minisat/core/Logger.h"
#include "minisat/core/ReduceKnownGraph.h"

namespace Minisat {

AcyclicSolverHelper::AcyclicSolverHelper(Polygraph *_polygraph) {
  // TODO: constructor of AcyclicSolverHelper
  polygraph = _polygraph;
  icd_graph.init(polygraph->n_vertices, polygraph->n_vars, polygraph);
  conflict_clauses.clear();
  added_edges_of.assign(polygraph->n_vars, {});
  dep_from.assign(polygraph->n_vertices, {});
  wr_from_keys.assign(polygraph->n_vertices, {});
  wr_from_of_key.assign(polygraph->n_vertices, {});
  wr_var_of_key.assign(polygraph->n_vertices, {});
  
  for (const auto &[from, to, type] : polygraph->known_edges) {
    icd_graph.add_known_edge(from, to);
    if (type == 0 || type == 1) { // SO or WR
      dep_from[to].insert({from, -1});
      if (type == 1) { // WR
        const auto &keys = polygraph->wr_keys.at(from).at(to);
        for (const auto &key : keys) {
          assert(!wr_from_keys[to].contains(key));
          wr_from_keys[to].insert(key);
          assert(!wr_from_of_key[to].contains(key) || wr_from_of_key[to][key] == -1);
          wr_from_of_key[to][key] = from;
          assert(!wr_var_of_key[to].contains(key) || wr_var_of_key[to][key] == -2); // -1 is used as known, use -2 instead...
          wr_var_of_key[to][key] = -1;
        }
      }
    } else { // CO
      assert(type == 2); 
    }
  }

  // * note: assume known graph has reached a fixed point, 
  // * i.e. no more CO edge can be induced here

  // ! deprecated
  // initialize vars_heap, sorting by n_edges_of_var
  int n_vars = polygraph->n_vars;
  for (int i = 0; i < n_vars; i++) vars_heap.insert({0, i});

  if (!icd_graph.preprocess()) {
    throw std::runtime_error{"Conflict found in Known Graph!"};
  }
}

// !deprecated
void AcyclicSolverHelper::add_var(int var) {
  // add_var adds the var into vars_heap
  int n_edges = 0;
  vars_heap.insert({n_edges, var});
  icd_graph.set_var_assigned(var, false);
} 

// !deprecated
void AcyclicSolverHelper::remove_var(int var) {
  // remove_var removes the var from vars_heap
  int n_edges = 0;
  if (vars_heap.contains({n_edges, var})) {
    vars_heap.erase({n_edges, var});
    icd_graph.set_var_assigned(var, true);
  }
}

void AcyclicSolverHelper::build_co(int var, std::vector<std::tuple<int, int, Reason>> &to_be_added_edges) {
  // TODO: build CO edges of var
  const auto &[from, to, key] = polygraph->wr_info[var];

  // 1. backward search starting from to
  auto build_co_dest_from = [&]() -> bool {
    bool reach_from = false;
    auto q = std::queue<std::pair<int, Reason>>{};
    auto vis = std::vector<bool>(polygraph->n_vertices, false);
    q.push({to, Reason{var}});
    vis[to] = true;
    while (!q.empty()) {
      const auto &[x, reason] = q.front();
      if (x == from) reach_from = true;
      if (x != to && x != from && polygraph->write_keys_of[x].contains(key)) {
        to_be_added_edges.emplace_back(x, from, reason);
      }
      for (const auto &[y, edge_var] : dep_from[x]) {
        if (vis[y]) continue;
        q.push({y, Reason{reason, edge_var}});
        vis[y] = true;
      }
      q.pop();
    }
    return reach_from;
  };

  // if from can be reached from to, then add (from, to) will not change the reachability of from and to
  //  there's no need to consider (from, to) as a part of (wr U so)+
  if (build_co_dest_from()) return;

  // 2. backward search starting from from
  {
    auto q = std::queue<std::pair<int, Reason>>{};
    auto vis = std::vector<bool>(polygraph->n_vertices, false);
    q.push({from, Reason{var}});
    vis[from] = true;
    while (!q.empty()) {
      const auto &[x, reason] = q.front();
      for (const auto &key : wr_from_keys[to]) {
        assert(wr_from_of_key[to].contains(key) && wr_from_of_key[to][key] != -1);
        int wr_from = wr_from_of_key[to][key];
        if (x != wr_from && polygraph->write_keys_of[x].contains(key)) {
          int wr_var = wr_var_of_key.at(to).at(key);
          to_be_added_edges.emplace_back(x, wr_from, Reason{reason, wr_var});
        }
      }
      for (const auto &[y, edge_var] : dep_from[x]) {
        if (vis[y]) continue;
        q.push({y, Reason{reason, edge_var}});
        vis[y] = true;
      }
      q.pop();
    }
  }
}

bool AcyclicSolverHelper::add_edges_of_var(int var) { 
  // TODO: add edges of var
  // return true if edge is successfully added into the graph, i.e. no cycle is detected 
  auto &added_edges = added_edges_of[var];
  assert(added_edges.empty());

  bool cycle = false;
  if (polygraph->is_wr_var(var)) {
    Logger::log(fmt::format("- adding {}, type = WR", var));

    auto to_be_added_edges = std::vector<std::tuple<int, int, Reason>>{};
    build_co(var, to_be_added_edges);

    // 1. add itself
    const auto &[from, to, key] = polygraph->wr_info[var];
    Logger::log(fmt::format(" - WR: {} -> {}, reason = {}, Itself", from, to, Reason{var}.to_string()));
    cycle = !icd_graph.add_edge(from, to, Reason{var});
    if (cycle) {
      Logger::log(" - conflict!");
      goto conflict;
    }
    Logger::log(" - success");
    added_edges.push({from, to, Reason{var}});
    
    // 2. add derived CO edges
    for (const auto &[from, to, reason] : to_be_added_edges) {
      Logger::log(fmt::format(" - CO: {} -> {}, reason = {}, Derived", from, to, reason.to_string()));
      cycle = !icd_graph.add_edge(from, to, reason);
      if (cycle) {
        Logger::log(" - conflict!");
        goto conflict; // bad implementation
      } 
      Logger::log(" - success");
      added_edges.push({from, to, reason});
    }

    if (!cycle) {
      assert(!dep_from[to].contains({from, var}));
      dep_from[to].insert({from, var});
      assert(!wr_from_keys[to].contains(key));
      wr_from_keys[to].insert(key);
      assert(!wr_from_of_key[to].contains(key) || wr_from_of_key[to][key] == -1);
      wr_from_of_key[to][key] = from;
      assert(!wr_var_of_key[to].contains(key) || wr_var_of_key[to][key] == -2);
      wr_var_of_key[to][key] = var;
      Logger::log(fmt::format(" - inserting ({} -> {}, key = {}) into solver_helper", from, to, key));
    } 
  } else {
    assert(false);
  }

  if (!cycle) {
    Logger::log(fmt::format(" - {} is successfully added", var));
    // disable icd_graph's get_propagated_lits temporarily
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
      Logger::log(fmt::format(" - removing edge {} -> {}, reason = {}", from, to, reason.to_string()));
      icd_graph.remove_edge(from, to, reason);
      added_edges.pop();
    }
    Logger::log(fmt::format(" - {} is not been added", var));
    assert(added_edges.empty());
    return false;
} 

void AcyclicSolverHelper::remove_edges_of_var(int var) {
  // TODO: remove edges of var
  Logger::log(fmt::format("- removing {}", var));

  auto &added_edges = added_edges_of[var];
  while (!added_edges.empty()) {
    const auto &[from, to, reason] = added_edges.top();
    icd_graph.remove_edge(from, to, reason);
    Logger::log(fmt::format(" - ??: {} -> {}, reason = {}", from, to, reason.to_string()));
    added_edges.pop();
  }
  // all edges have been deleted including CO and WR(itself)
  if (polygraph->is_wr_var(var)) { 
    const auto &[from, to, key] = polygraph->wr_info[var];
    assert(dep_from[to].contains({from, var}));
    dep_from[to].erase({from, var});
    assert(wr_from_keys[to].contains(key));
    wr_from_keys[to].erase(key);
    assert(wr_from_of_key[to][key] == from);
    wr_from_of_key[to][key] = -1;
    assert(wr_var_of_key[to][key] == var);
    wr_var_of_key[to][key] = -2;

    Logger::log(fmt::format(" - deleting ({} -> {}, key = {}) in solver_helper", from, to, key));
  } else {
    assert(false);
  }

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

  // auto get_or_allocate = [&](int v1, int v2) -> CRef {
  //   if (v1 > v2) std::swap(v1, v2);
  //   if (allocated_unique_clause.contains(v1) && allocated_unique_clause[v1].contains(v2)) {
  //     return allocated_unique_clause[v1][v2];
  //   }
  //   vec<Lit> lits;
  //   lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
  //   CRef cr = ca->alloc(lits, false);
  //   return allocated_unique_clause[v1][v2] = cr;
  // };

  auto mapped = [&](int v1, int v2) -> bool {
    if (v1 > v2) std::swap(v1, v2);
    if (allocated_unique_clause.contains(v1) && allocated_unique_clause[v1].contains(v2) && allocated_unique_clause[v1][v2]) {
      return true;
    }
    allocated_unique_clause[v1][v2] = true;
    return false;
  };

  for (const auto &var2 : *wr_cons_ref) {
    if (icd_graph.get_var_assigned(var2)) continue;
    // if (mapped(var, var2)) continue;
    Logger::log(fmt::format(" - prop (~{}) with reason (~{} | ~{})", var2, var, var2));
    propagated_lits.emplace_back(std::pair<Lit, std::vector<Lit>>{~mkLit(var2), {~mkLit(var), ~mkLit(var2)}});
    // propagated_lits.emplace_back(std::pair<Lit, CRef>{~mkLit(var2), get_or_allocate(var, var2)});
  }
  Logger::log(fmt::format("- end of WRCP construction", var));
#endif
}

Polygraph *AcyclicSolverHelper::get_polygraph() { return polygraph; }

const int AcyclicSolverHelper::get_level(int x) const { return icd_graph.get_level(x); }

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