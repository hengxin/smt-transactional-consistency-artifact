#include "AcyclicSolverHelper.h"

#include <iostream>
#include <vector>
#include <tuple>

#include "minisat/core/Polygraph.h"
#include "minisat/core/ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"

namespace Minisat {

AcyclicSolverHelper::AcyclicSolverHelper(Polygraph *_polygraph) {
  polygraph = _polygraph;
  icd_graph.init(polygraph->n_vertices);
  conflict_clauses.clear();
  reach_graph.init(polygraph->n_vertices);

  for (const auto &[from, to] : polygraph->known_edges) {
    icd_graph.add_edge(from, to, /* label = */ -1, /* need_detect_cycle = */ false);  // return value is ignored here
    reach_graph.add_edge(from, to, /* need_count = */ false);
  }

  reach_graph.init_reachability();

  // initialize vars_heap, sorting by n_edges_of_var
  int n_vars = polygraph->edges.size();
  for (int i = 0; i < n_vars; i++) vars_heap.insert(std::make_pair(polygraph->edges[i].size(), i));
}

void AcyclicSolverHelper::add_var(int var) {
  int n_edges = polygraph->edges[var].size();
  if (vars_heap.contains(std::make_pair(n_edges, var))) {
    vars_heap.erase(std::make_pair(n_edges, var));
  }
} 

void AcyclicSolverHelper::remove_var(int var) {
  int n_edges = polygraph->edges[var].size();
  vars_heap.insert(std::make_pair(n_edges, var));
}

bool AcyclicSolverHelper::detect_cycle_in_proximity(int var) {
  // 1. pruning, width = 1
  const auto &edges_of_var = polygraph->edges[var];
  for (const auto &[from, to] : edges_of_var) {
    if (reach_graph.can_reach(to, from)) {
      conflict_clause_in_proximity.clear();
      conflict_clause_in_proximity.push_back(mkLit(var, false));
      return true;
    }
  }

  // TODO: more length: pair, triple, ...

  return false;
} 

bool AcyclicSolverHelper::add_edges_of_var(int var) { 
  // return true if edge is successfully added into the graph, i.e. no cycle is detected 
  const auto &edges_of_var = polygraph->edges[var];
  bool cycle = false;
  std::vector<std::tuple<int, int, int>> added_edges;

  // TODO: refactor this function, to integrate reach_graph 

  // * note: This(below) is a bad implementation! It's better to integrate icd_graph and reach_graph in a unified function.
  cycle = detect_cycle_in_proximity(var);
  if (cycle) {
    conflict_clauses.push_back(conflict_clause_in_proximity);
    conflict_clause_in_proximity.clear();
    return false;
  }

  for (const auto &[from, to] : edges_of_var) {
    cycle = !icd_graph.add_edge(from, to, /* label = */ var);
    if (cycle) break;
    added_edges.push_back(std::make_tuple(from, to, var));
  }
  if (!cycle) {
    for (const auto &[from, to] : edges_of_var) {
      reach_graph.add_edge(from, to);
    }
    icd_graph.get_propagated_lits(propagated_lits);
    return true;
  } else {
    std::vector<Lit> cur_conflict_clause;
    icd_graph.get_minimal_cycle(cur_conflict_clause);
    conflict_clauses.push_back(cur_conflict_clause);
    for (const auto &[from, to, label] : added_edges) {
      icd_graph.remove_edge(from, to, label);
    }
    return false;
  }
} 

void AcyclicSolverHelper::remove_edges_of_var(int var) {
  const auto &edges_of_vars = polygraph->edges[var];
  for (const auto &[from, to] : edges_of_vars) {
    icd_graph.remove_edge(from, to, /* label = */ var);
    reach_graph.remove_edge(from, to);
  }
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

} // namespace Minisat