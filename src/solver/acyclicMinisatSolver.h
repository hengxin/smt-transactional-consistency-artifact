#ifndef CHECKER_SOLVER_ACYCLICMINISATSOLVER_H
#define CHECKER_SOLVER_ACYCLICMINISATSOLVER_H

#include <vector>
#include <utility>
#include <filesystem>

#include <acyclic_minisat.h>

#include "abstractSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"

using checker::history::EdgeType;

namespace fs = std::filesystem;

namespace checker::solver {

struct AcyclicMinisatSolver : AbstractSolver {
  using SimplifiedEdge = std::tuple<int, int, int>;
  using SimplifiedKnownGraph = std::vector<SimplifiedEdge>;
  using SimplifiedEdgeSet = std::vector<SimplifiedEdge>;
  using SimplifiedConstraint = std::pair<SimplifiedEdgeSet, SimplifiedEdgeSet>;
  using SimplifiedConstraints = std::vector<SimplifiedConstraint>;

  fs::path agnf_path;

  int n_vertices;

  SimplifiedKnownGraph simplified_known_graph;
  SimplifiedConstraints simplified_constraints;
  // std::vector<std::pair<SimplifiedKnownGraph, SimplifiedConstraints>> polygraph_components;

  AcyclicMinisatSolver(const history::DependencyGraph &known_graph,
                       const std::vector<history::Constraint> &constraints);

  auto solve() -> bool override;

  ~AcyclicMinisatSolver();
};

} // namespace checker::solver

#endif