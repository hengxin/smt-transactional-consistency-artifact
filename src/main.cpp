#include <z3++.h>

#include <iostream>

int main(int argc, char **argv) {
  z3::context c;
  auto x = c.bool_const("x");
  auto y = c.bool_const("y");
  auto conj = (!(x && y)) == (!x || !y);

  z3::solver s{c};
  s.add(!conj);

  switch (s.check()) {
    case z3::unsat:
      std::cout << "Hello z3!" << std::endl;
      break;
    default:
      std::cout << "BUG!" << std::endl;
      return 1;
  }
  return 0;
}
