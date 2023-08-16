#include <filesystem>

#include "acyclicMinisatSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/agnf.h"

namespace fs = std::filesystem;

namespace checker::solver {

AcyclicMinisatSolver::AcyclicMinisatSolver(const history::DependencyGraph &known_graph,
                                           const std::vector<history::Constraint> &constraints) {
  agnf_path = fs::current_path();
  agnf_path.append("acyclicminisat_tmp_input.agnf");
  try {
    utils::write_to_agnf_file(agnf_path, known_graph, constraints);
  } catch (std::runtime_error &e) {
    // TODO   
  }
}

auto AcyclicMinisatSolver::solve() -> bool {
  // TODO: call acyclic-minisat as backend solver
  return true;
}

AcyclicMinisatSolver::~AcyclicMinisatSolver() = default;

} // namespace checker::solver