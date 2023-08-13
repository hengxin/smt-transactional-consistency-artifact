#ifndef CHECKER_UTILS_GNF_H
#define CHECKER_UTILS_GNF_H 

#include <iostream>
#include <cstdio>
#include <string>
#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace checker::utils {
bool write_to_gnf_file(std::FILE *fp, 
                      const history::DependencyGraph &known_graph,
                      const std::vector<history::Constraint> &constraints) {
  // TODO: write into file, return true if success
  return true;
}
} // namespace

#endif // CHECKER_UTILS_GNF_H