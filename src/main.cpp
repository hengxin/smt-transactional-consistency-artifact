#include <z3++.h>

#include <argparse/argparse.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "history/history.h"
#include "history/dependencygraph.h"
#include "history/constraint.h"

namespace history = checker::history;

using std::cout;

int main(int argc, char** argv) {
  z3::context c;
  auto x = c.bool_const("x");
  auto y = c.bool_const("y");
  auto conj = (!(x && y)) == (!x || !y);

  z3::solver s{c};
  s.add(!conj);

  auto args = argparse::ArgumentParser{"checker", "0.0.1"};
  args.add_argument("history").help("History file");

  try {
    args.parse_args(argc, argv);
  } catch(const std::runtime_error &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  switch (s.check()) {
    case z3::unsat:
      std::cerr << "Hello z3!\n";
      break;
    default:
      std::cerr << "BUG!\n";
      return 1;
  }

  std::ifstream history_file{args.get("history")};
  auto history = history::parse_dbcop_history(history_file);
  auto dependency_graph = history::known_graph_of(history);
  auto constraints = history::constraints_of(history, dependency_graph.wr);

  cout << history;

  cout << "RW:\n" << dependency_graph.rw;
  cout << "WW:\n" << dependency_graph.ww;
  cout << "SO:\n" << dependency_graph.so;
  cout << "WR:\n" << dependency_graph.wr;

  for (const auto &c : constraints) {
    cout << c;
  }

  return 0;
}
