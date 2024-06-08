#ifndef MINISAT_PAIRCONFLICT_H
#define MINISAT_PAIRCONFLICT_H

#include <iostream>

#include <fmt/format.h>

#include "minisat/core/Polygraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/core/Solver.h"
#include "minisat/core/AcyclicSolver.h"
#include "minisat/core/Graph.h"
#include "minisat/core/Logger.h"

namespace Minisat {

bool init_pair_conflict(AcyclicSolver &solver) {
  Logger::log("[Init Pair Conflict]");
  Polygraph *polygraph = solver.get_polygraph();
  if (polygraph->n_vertices > 200000) {
    Logger::log(fmt::format(" - failed!  polygraph has {} vertices, > limit 100000", polygraph->n_vertices));
    return false;
  }
  if (polygraph->n_vars > 100000) {
    Logger::log(fmt::format(" - failed! solver has {} vars, > limit 10000", polygraph->n_vars));
    return false;
  }

  Graph graph = Graph(polygraph->n_vertices);
  for (const auto &[from, to, _] : polygraph->known_edges) {
    graph.add_edge(from, to); // Graph helps handle duplicated edges
  }

  auto edge = [&polygraph](int v) -> std::pair<int, int> {
    // * note: hard to extend to RW edges
    if (polygraph->is_wr_var(v)) {
      auto &&[from, to, _] = polygraph->wr_info[v];
      return {from, to};
    } 
    assert(false);
  };

  auto conflict = [&edge, &graph, &polygraph](int v1, int v2) -> bool {
    auto [from1, to1] = edge(v1);
    auto [from2, to2] = edge(v2);

    // conflict case I: 2 edges
    if (graph.reachable(to1, from2) && graph.reachable(to2, from1)) {
      return true;
    }

    // conflict case II: 1 RW edge
    // if (from1 == from2) {
    //   if (polygraph->is_wr_var(v1) && polygraph->is_ww_var(v2)) {
    //     std::swap(v1, v2);
    //     std::swap(to1, to2);
    //     // from1 = from2
    //   } 
    //   if (polygraph->is_ww_var(v1) && polygraph->is_wr_var(v2) && to1 != to2) {
    //     // derive RW edge: to2 -> to1
    //     const auto &[_1, _2, wr_key] = polygraph->wr_info[v2];
    //     const auto &[_3, _4, ww_keys] = polygraph->ww_info[v1];
    //     if (ww_keys.contains(wr_key) && graph.reachable(to1, to2)) return true;
    //   }
    // } 

    return false;
  };

  auto self_conflict = [&edge, &graph](int v) -> bool {
    // self_conflict will be True only if v is a RW variable when OUTER_RW_DERIVATION is enabled
    auto [from, to] = edge(v);
    return graph.reachable(to, from);
  };

  for (int v1 = 0; v1 < polygraph->n_vars; v1++) {
    #ifdef OUTER_RW_DERIVATION
      if (self_conflict(v1)) { // checking of v2 will also be included
        vec<Lit> lits;
        lits.push(~mkLit(v1));
        solver.addClause_(lits); // here solver_helper has been initialized
        Logger::log(Logger::lits2str(lits));
      }
    #endif
    for (int v2 = v1 + 1; v2 < polygraph->n_vars; v2++) {
      if (conflict(v1, v2)) {
        vec<Lit> lits;
        lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
        solver.addClause_(lits); 
        Logger::log(Logger::lits2str(lits));
      }
    }
  }

  return true;
}

// void init_pair_conflict(Polygraph *polygraph, Solver &solver) {
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
// }

} // namespace Minisat

#endif // MINISAT_PAIR_CONFLICT_H