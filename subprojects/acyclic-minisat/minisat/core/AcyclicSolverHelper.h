#ifndef MINISAT_ACYCLICSOLVER_HELPER
#define MINISAT_ACYCLICSOLVER_HELPER

#include <vector>
#include <set>
#include <utility>

#include "minisat/core/Polygraph.h"
#include "minisat/core/ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"
#include "minisat/core/DependencyGraph.h"

// manage polygraph and ICDGraph

namespace Minisat {

class AcyclicSolverHelper {
  Polygraph *polygraph;
  ICDGraph induced_graph;
  DependencyGraph dep_graph, antidep_graph;

  bool add_dep_edge(int from, int to, EdgeType type, int var, bool is_known_edge = false);
  void remove_dep_edge(int from, int to, EdgeType type, int var);

public:
  std::vector<std::vector<Lit>> conflict_clauses;
  AcyclicSolverHelper(Polygraph *_polygraph);
  bool add_edges_of_var(int var);
  void remove_edges_of_var(int var);

}; // class AcyclicSolverHelper

} // namespace Minisat

#endif // MINISAT_ACYCLICSOLVER_HELPER