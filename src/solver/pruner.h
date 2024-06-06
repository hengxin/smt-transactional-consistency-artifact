#ifndef CHECKER_SOLVER_PRUNER_H
#define CHECKER_SOLVER_PRUNER_H

#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace checker::solver {
auto prune_constraints(history::DependencyGraph &dependency_graph,
                       history::Constraints &constraints) -> bool;

}

#endif  // CHECKER_SOLVER_PRUNER_H
