#include "monosatSolver.h"

#include <Monosat.h>

#include <vector>
#include <string>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <boost/log/trivial.hpp>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/gnf.h"

namespace fs = std::filesystem;

namespace checker::solver {

MonosatSolver::MonosatSolver(const history::DependencyGraph &known_graph,
                            const std::vector<history::Constraint> &constraints) {
  solver = newSolver();
  gnf_path = fs::current_path();
  gnf_path.append("monosat_tmp_input.gnf");
  try {
    utils::write_to_gnf_file(gnf_path, known_graph, constraints);
  } catch (std::runtime_error &e) {
    // TODO
  };
  
}

auto MonosatSolver::solve() -> bool {
  // linux specific method to get the tmpfile name

  BOOST_LOG_TRIVIAL(debug)
    << "generating tmp file "
    << gnf_path;

  readGNF(solver, gnf_path.c_str());
  bool ret = solveWrapper(solver);
  return ret;
}

MonosatSolver::~MonosatSolver() { deleteSolver(solver); };

}