#ifndef CHECKER_SOLVER_ACYCLICMINISATSOLVER_H
#define CHECKER_SOLVER_ACYCLICMINISATSOLVER_H

#include <vector>
#include <utility>
#include <filesystem>

#include "abstractSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace fs = std::filesystem;

namespace checker::solver {

struct AcyclicMinisatSolver : AbstractSolver {
  using SimplifiedEdge = std::pair<int, int>;
  using SimplifiedKnownGraph = std::vector<SimplifiedEdge>;
  using SimplifiedEdgeSet = std::vector<SimplifiedEdge>;
  using SimplifiedConstraint = std::pair<SimplifiedEdgeSet, SimplifiedEdgeSet>;
  using SimplifiedConstraints = std::vector<SimplifiedConstraint>;

  fs::path agnf_path;

  std::vector<std::pair<SimplifiedKnownGraph, SimplifiedConstraints>> polygraph_components;

  AcyclicMinisatSolver(const history::DependencyGraph &known_graph,
                       const std::vector<history::Constraint> &constraints);

  auto solve() -> bool override;

  ~AcyclicMinisatSolver();
};

} // namespace checker::solver

#endif