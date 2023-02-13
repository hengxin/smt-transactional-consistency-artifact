#include <z3++.h>

#include <argparse/argparse.hpp>
#include <boost/log/trivial.hpp>
#include <fstream>
#include <ios>
#include <iostream>
#include <stdexcept>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "history/history.h"
#include "solver/solver.h"

namespace history = checker::history;
namespace solver = checker::solver;

using std::cout;

auto main(int argc, char **argv) -> int {
  auto args = argparse::ArgumentParser{"checker", "0.0.1"};
  args.add_argument("history").help("History file");

  try {
    args.parse_args(argc, argv);
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  auto history_file = std::ifstream{args.get("history")};
  auto history = history::parse_dbcop_history(history_file);
  auto dependency_graph = history::known_graph_of(history);
  auto constraints = history::constraints_of(history, dependency_graph.wr);

  cout << history;

  BOOST_LOG_TRIVIAL(trace) << "RW:\n"
                           << dependency_graph.rw << '\n'
                           << "WW:\n"
                           << dependency_graph.ww << '\n'
                           << "SO:\n"
                           << dependency_graph.so << '\n'
                           << "WR:\n"
                           << dependency_graph.wr;

  for (const auto &c : constraints) {
    BOOST_LOG_TRIVIAL(trace) << c;
  }

  auto solver = solver::Solver{dependency_graph, constraints};
  cout << std::ios::boolalpha << solver.solve() << '\n';

  return 0;
}
