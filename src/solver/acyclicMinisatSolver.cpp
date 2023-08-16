#include <filesystem>

#include "acyclicMinisatSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/agnf.h"
#include "utils/subprocess.hpp"

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
  // a toy implementation: call acyclic-minisat as a subprocess
  auto obuf = subprocess::check_output({"/home/rikka/acyclic-minisat/build/minisat_core", agnf_path});
  BOOST_LOG_TRIVIAL(debug) << "stdout of acyclic-minisat: " << obuf.buf.data();
  std::string output = "";
  for (char ch : obuf.buf) output += ch;
  output = output.substr(0, output.length() - 1); // delete last '\n'
  return output == "SAT";
}

AcyclicMinisatSolver::~AcyclicMinisatSolver() = default;

} // namespace checker::solver