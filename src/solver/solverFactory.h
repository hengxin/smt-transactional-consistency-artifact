#ifndef CHECKER_SOLVER_FACTORY_H
#define CHECKER_SOLVER_FACTORY_H

#include <string>

#include "history/constraint.h"
#include "history/dependencygraph.h"

#include "solver/abstractSolver.h"
#include "solver/z3Solver.h"
// TODO: implement more solvers

namespace checker::solver {
  struct SolverFactory {
private:    
    static const SolverFactory solverFactory;

public:
    auto make(const std::string &solver_type, 
              const history::DependencyGraph &dependency_graph,
              const std::vector<history::Constraint> &constraints) -> AbstractSolver {
                if (solver_type == "z3") {
                  return Z3Solver{dependency_graph, constraints};
                } else {
                  throw std::runtime_error("unknown solver!");
                }
              }

    static auto getSolverFactory() -> SolverFactory { return solverFactory; }
  };
}

#endif
