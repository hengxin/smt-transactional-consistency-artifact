#ifndef MINISAT_ACYCLICSOLVER_HELPER
#define MINISAT_ACYCLICSOLVER_HELPER

#include <vector>
#include <set>
#include <utility>

#include "minisat/core/Polygraph.h"
#include "minisat/core/ReachGraph.h"
#include "minisat/core/ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"

// manage polygraph and ICDGraph

namespace Minisat {

class AcyclicSolverHelper {
  Polygraph *polygraph;
  ICDGraph icd_graph;
  ReachGraph reach_graph;
  std::set<std::pair<int, int>> vars_heap;
  std::vector<Lit> conflict_clause_in_proximity;

  bool detect_cycle_in_proximity(int var);

public:
  std::vector<std::vector<Lit>> conflict_clauses;
  std::vector<Lit> propagated_lits;
  AcyclicSolverHelper(Polygraph *_polygraph);
  void add_var(int var);
  void remove_var(int var);
  bool add_edges_of_var(int var);
  void remove_edges_of_var(int var);
  Var get_var_represents_max_edges();
  Var get_var_represents_min_edges();

}; // class AcyclicSolverHelper

} // namespace Minisat

#endif // MINISAT_ACYCLICSOLVER_HELPER