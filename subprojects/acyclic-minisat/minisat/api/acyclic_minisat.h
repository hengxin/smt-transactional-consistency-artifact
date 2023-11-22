#ifndef MINISAT_ACYCLIC_MINISAT_H
#define MINISAT_ACYCLIC_MINISAT_H

#include <utility>
#include <vector>

#include "minisat/core/Solver.h"
#include "minisat/core/Polygraph.h"
#include "minisat/core/AcyclicSolver.h"
#include "minisat/core/AcyclicSolverHelper.h"
#include "minisat/core/Constructor.h"

namespace Minisat {

using Edge = std::tuple<int, int, EdgeType>;
using KnownGraph = std::vector<Edge>;
using EdgeSet = std::vector<Edge>;
using Constraint = std::pair<EdgeSet, EdgeSet>;
using Constraints = std::vector<Constraint>;

// precondition: known_graph and constraints must be remapped
bool am_solve(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints);

} // namespace Minisat

#endif // MINISAT_ACYCLIC_MINISAT_H