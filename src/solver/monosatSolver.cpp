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
  input_file = std::tmpfile();
  utils::write_to_gnf_file(input_file, known_graph, constraints);
}

auto MonosatSolver::solve() -> bool {
  // linux specific method to get the tmpfile name
  std::string input_file_path = fs::read_symlink(fs::path("/proc/self/fd") 
                                / std::to_string(fileno(input_file)));

  BOOST_LOG_TRIVIAL(debug)
    << "generating tmp file '"
    << input_file_path
    << "'";

  return true;

  // readGNF(solver, input_file_path.c_str());
  // bool ret = solveWrapper(solver);
}

MonosatSolver::~MonosatSolver() { std::fclose(input_file); };
}