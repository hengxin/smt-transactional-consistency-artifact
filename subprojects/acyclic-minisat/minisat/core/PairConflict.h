#ifndef MINISAT_PAIRCONFLICT_H
#define MINISAT_PAIRCONFLICT_H

#include <iostream>

#include "minisat/core/Polygraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/core/Solver.h"
#include "minisat/core/Graph.h"

namespace Minisat {

void init_pair_conflict(Polygraph *polygraph, Solver &solver) {
  // TODO: init pair conflict
  // ! deprecated
  // if (polygraph->n_vertices > 100000)  {
  //   std::cerr << "skip pair conflict generation: too many vertices" << std::endl;
  //   return; // skip this optimization when there are too many vertices
  // }
  // if (polygraph->edges.size() > 10000) {
  //   std::cerr << "skip pair conflict generation: too many constraints" << std::endl;
  //   return; // skip this optimization when there are too many constraints
  // } 
  // Graph graph = Graph(polygraph->n_vertices);
  // // add all edges of known graph
  // for (const auto &[from, to] : polygraph->known_edges) { graph.add_edge(from, to); }
  // auto conflict = [&](int v1, int v2) -> bool {
  //   if (polygraph->edges[v1].empty() || polygraph->edges[v2].empty()) return false;
  //   for (const auto &[from1, to1] : polygraph->edges[v1]) {
  //     for (const auto &[from2, to2] : polygraph->edges[v2]) {
  //       if (graph.reachable(to1, from2) && graph.reachable(to2, from1)) {
  //         return true;
  //       }
  //       return false;
  //     }
  //   }
  //   return false;
  //   // const auto &[from1, to1] = polygraph->edges[v1][0];
  //   // const auto &[from2, to2] = polygraph->edges[v2][0];
  //   // if (graph.reachable(to1, from2) && graph.reachable(to2, from1)) return true;
  //   // return false;
  // };
  // auto possible_triple_conflict = [&](int v1, int v2) -> bool {
  //   // TODO
  //   return false;
  //   if (polygraph->edges[v1].empty() || polygraph->edges[v2].empty()) return false;
  //   for (const auto &[from1, to1] : polygraph->edges[v1]) {
  //     for (const auto &[from2, to2] : polygraph->edges[v2]) {
  //       if (graph.reachable(to1, from2) && graph.reachable(to2, from1)) return false;
  //       if (graph.reachable(to1, from2) || graph.reachable(to2, from1)) return true;
  //       return false;
  //     }
  //   }
  //   return false;
  // };
  // // auto triple_conflict = // TODO
  // int n_vars = polygraph->edges.size();
  // for (int v1 = 0; v1 < n_vars; v1++) {
  //   for (int v2 = v1 + 1; v2 < n_vars; v2++) {
  //     if (conflict(v1, v2)) {
  //       vec<Lit> lits;
  //       lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
  //       solver.addClause_(lits);
  //     } else if (possible_triple_conflict(v1, v2)) {
  //       // TODO
  //     }
  //   }
  // }
}

} // namespace Minisat

#endif // MINISAT_PAIR_CONFLICT_H