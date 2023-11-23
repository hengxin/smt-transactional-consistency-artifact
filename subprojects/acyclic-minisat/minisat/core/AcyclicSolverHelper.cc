#include "AcyclicSolverHelper.h"

#include <iostream>
#include <vector>
#include <stack>
#include <bitset>
#include <tuple>
#include <cassert>

#include "minisat/core/Polygraph.h"
#include "minisat/core/ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"
#include "minisat/core/OptOption.h"

namespace Minisat {

AcyclicSolverHelper::AcyclicSolverHelper(Polygraph *_polygraph) {
  polygraph = _polygraph;
  auto n = polygraph->n_vertices;
  induced_graph.init(n, polygraph->edges.size());
  dep_graph.init(n), antidep_graph.init(n);
  for (int i = 0; i < n; i++) antidep_graph.add_edge(i, i, /* var = */ -1); // init relexive closure of antidep_graph

  for (const auto &[from, to, type] : polygraph->known_edges) {
    add_dep_edge(from, to, type, /* label = */ -1, true); // add known edges
  }
}

bool AcyclicSolverHelper::add_dep_edge(int from, int to, EdgeType type, int var, bool is_known_edge) {
  struct InducedEdge { int from, to, dep_reason, antidep_reason; };
  auto added_induced_edges = std::stack<InducedEdge>{};
  bool cycle = false;
  if (type == EdgeType::RW) {
    if (antidep_graph.contains_edge(from, to, var)) {
      assert(is_known_edge);
      return true;
    } 
    for (const auto &[pred, pred_var] : dep_graph.pred.at(from)) {
      if (is_known_edge) cycle = !induced_graph.add_known_edge(pred, to, pred_var, var);
      else cycle = !induced_graph.add_edge(pred, to, pred_var, var);
      if (cycle) break;
      added_induced_edges.push(InducedEdge{pred, to, pred_var, var});
    }
    if (!cycle) {
      antidep_graph.add_edge(from, to, var);
    }
  } else { // SO, WW, WR
    if (dep_graph.contains_edge(from, to, var)) {
      assert(is_known_edge);
      return true;
    } 
    for (const auto &[succ, succ_var] : antidep_graph.succ.at(to)) {
      cycle = !induced_graph.add_edge(from, succ, var, succ_var);
      if (cycle) break;
      added_induced_edges.push(InducedEdge{from, succ, var, succ_var});
    }
    if (!cycle) {
      dep_graph.add_edge(from, to, var);
    }
  }
  if (cycle) {
    assert(!is_known_edge);
    while (!added_induced_edges.empty()) {
      const auto &[from, to, dep_reason, antidep_reason] = added_induced_edges.top();
      induced_graph.remove_edge(from, to, dep_reason, antidep_reason);
      added_induced_edges.pop();
    }
    return false;
  }
  return true;
}

void AcyclicSolverHelper::remove_dep_edge(int from, int to, EdgeType type, int var) {
  assert(type == EdgeType::WW || type == EdgeType::RW);
  if (type == EdgeType::RW) {
    assert(antidep_graph.contains_edge(from, to, var));
    for (const auto &[pred, pred_var] : dep_graph.pred.at(from)) {
      induced_graph.remove_edge(pred, to, pred_var, var);
    }
    antidep_graph.remove_edge(from, to, var);
  } else { // WW
    assert(dep_graph.contains_edge(from, to, var));
    for (const auto &[succ, succ_var] : antidep_graph.succ.at(to)) {
      induced_graph.remove_edge(from, succ, var, succ_var);
    }
    dep_graph.remove_edge(from, to, var);
  }
} 

bool AcyclicSolverHelper::add_edges_of_var(int var) { 
  // return true if edge is successfully added into the graph, i.e. no cycle is detected 
  struct DepGraphEdge { int from, to; EdgeType type; int var; }; // note: to distingguish from DepEdge
  auto added_dep_edges = std::stack<DepGraphEdge>{};
  auto cycle = false;
  const auto &edges_of_var = polygraph->edges[var];
  for (const auto &[from, to, type] : edges_of_var) {
    cycle = !add_dep_edge(from, to, type, var);
    if (cycle) break;
    added_dep_edges.push((DepGraphEdge){from, to, type, var});
  }
  if (!cycle) return true;
  std::vector<Lit> cur_conflict_clause;
  induced_graph.get_minimal_cycle(cur_conflict_clause);
  conflict_clauses.emplace_back(cur_conflict_clause);
  while (!added_dep_edges.empty()) {
    const auto &[from, to, type, var] = added_dep_edges.top();
    remove_dep_edge(from, to, type, var);
    added_dep_edges.pop();
  }
  return false;
} 

void AcyclicSolverHelper::remove_edges_of_var(int var) {
  const auto &edges_of_var = polygraph->edges[var];
  for (const auto &[from, to, type] : edges_of_var) {
    remove_dep_edge(from, to, type, var);
  }
}

} // namespace Minisat