#ifndef CHECKER_SOLVER_SOLVER_H
#define CHECKER_SOLVER_SOLVER_H

#include <z3++.h>

#include <memory>
#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace checker::solver {
struct DependencyGraphHasNoCycle;

struct Solver {
  z3::context context;
  z3::solver solver;
  std::unique_ptr<DependencyGraphHasNoCycle> user_propagator;
  z3::expr_vector known_vars;

  Solver(const history::DependencyGraph &known_graph,
         const std::vector<history::Constraint> &constraints);

  auto solve() -> bool;

  ~Solver();
};
}  // namespace checker::solver

#endif  // CHECKER_SOLVER_SOLVER_H
