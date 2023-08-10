#ifndef CHECKER_SOLVER_SOLVER_H
#define CHECKER_SOLVER_SOLVER_H

#include <z3++.h>

#include <memory>
#include <vector>

#include "abstractSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace checker::solver {
struct DependencyGraphHasNoCycle;

struct Z3Solver : AbstractSolver {
  z3::context context;
  z3::solver solver;
  std::unique_ptr<DependencyGraphHasNoCycle> user_propagator;

  Z3Solver(const history::DependencyGraph &known_graph,
         const std::vector<history::Constraint> &constraints);

  auto solve() -> bool override;

  ~Z3Solver();
};
}  // namespace checker::solver

#endif  // CHECKER_SOLVER_SOLVER_H
