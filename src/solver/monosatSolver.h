#ifndef CHECKER_SOLVER_MONOSATSOLVER_H
#define CHECKER_SOLVER_MONOSATSOLVER_H

#include <Monosat.h>

#include <vector>
#include <string>
#include <iostream>

#include "abstractSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace checker::solver {
struct MonosatSolver : AbstractSolver {
  SolverPtr solver;
  std::FILE* input_file;

  MonosatSolver(const history::DependencyGraph &known_graph,
                const std::vector<history::Constraint> &constraints);

  auto solve() -> bool override;

  ~MonosatSolver();
};
} // namespace

#endif // CHECKER_SOLVER_MONOSATSOLVER_H