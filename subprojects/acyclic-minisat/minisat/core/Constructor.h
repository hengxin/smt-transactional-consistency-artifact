#ifndef MINISAT_CONSTRUCTOR_H
#define MINISAT_CONSTRUCTOR_H

#include <cassert>
#include <tuple>

#include "minisat/core/EdgeType.h"
#include "minisat/core/Polygraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/core/Graph.h"
#include "minisat/core/Solver.h"
#include "minisat/core/PairConflict.h"
#include "minisat/core/OptOption.h"

namespace Minisat {

using Edge = std::tuple<int, int, int>;
using KnownGraph = std::vector<Edge>;
using EdgeSet = std::vector<Edge>;
using Constraint = std::pair<EdgeSet, EdgeSet>;
using Constraints = std::vector<Constraint>;

// construct si constraints
Polygraph *construct(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints, Solver &solver) {
  int n_constraints = constraints.size();
  Polygraph *polygraph = new Polygraph(n_vertices, n_constraints * 2);
  auto get_minisat_type = [](int t) -> EdgeType {
    switch (t) {
      case 0: return EdgeType::SO;
      case 1: return EdgeType::WW;
      case 2: return EdgeType::WR;
      case 3: return EdgeType::RW;
      default: assert(0);
    }
  };
  for (const auto &[from, to, type] : known_graph) {
    polygraph->add_known_edge(from, to, get_minisat_type(type));
  }
  int var_count = 0;
  for (const auto &[either_, or_] : constraints) {
    auto add_constraint = [&](const EdgeSet &edges) -> void {
      for (const auto &[from, to, type] : edges) {
        polygraph->add_constraint_edge(var_count, from, to, get_minisat_type(type));
      }
      ++var_count;
    };
    add_constraint(either_), add_constraint(or_);
  }
  assert(var_count == n_constraints * 2);
  for (int i = 0; i < var_count; i += 2) {
    solver.newVar(), solver.newVar();
    int v1 = i, v2 = i + 1;
    vec<Lit> lits;
    lits.push(mkLit(v1)), lits.push(mkLit(v2));
    solver.addClause_(lits);
    lits.clear(), lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
    solver.addClause_(lits);
  }
  return polygraph;
}

// construct ser constraints
// Polygraph *construct(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints, Solver &solver) {
//   int n_constraints = constraints.size();
//   Polygraph *polygraph = new Polygraph(n_vertices, n_constraints * 2);
//   for (const auto &[from, to] : known_graph) {
//     polygraph->add_known_edge(from, to);
//   }
//   int var_count = 0;
//   for (const auto &[either_, or_] : constraints) {
//     auto add_constraint = [&](const EdgeSet &edges) -> void {
//       for (const auto &[from, to] : edges) {
//         polygraph->add_constraint_edge(var_count, from, to);
//       }
//       ++var_count;
//     };
//     add_constraint(either_), add_constraint(or_);
//   }
//   assert(var_count == n_constraints * 2);
//   for (int i = 0; i < var_count; i += 2) {
//     solver.newVar(), solver.newVar();
//     int v1 = i, v2 = i + 1;
//     vec<Lit> lits;
//     lits.push(mkLit(v1)), lits.push(mkLit(v2));
//     solver.addClause_(lits);
//     lits.clear(), lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
//     solver.addClause_(lits);
//   }

// #ifdef INIT_PAIR_CONFLICT
//   init_pair_conflict(polygraph, solver);
// #endif

//   return polygraph;
// }

} // namespace Minisat

#endif // MINISAT_CONSTRUCTOR_H