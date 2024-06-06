#ifndef MINISAT_ACYCLIC_MINISAT_H
#define MINISAT_ACYCLIC_MINISAT_H

#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "minisat/api/acyclic_minisat_types.h"

namespace Minisat {

// precondition: known_graph and constraints must be remapped
bool am_solve(int n_vertices, 
              const KnownGraph &known_graph, 
              const Constraints &constraints,
              const std::unordered_map<int, std::unordered_set<int64_t>> &write_keys_of
              );

} // namespace Minisat

#endif // MINISAT_ACYCLIC_MINISAT_H