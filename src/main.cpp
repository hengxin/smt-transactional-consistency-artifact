#include <z3++.h>

#include <argparse/argparse.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "history/history.h"

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
  auto history = checker::history::History::parse_dbcop_history(history_file);
  std::cout << history;

  return 0;
}
