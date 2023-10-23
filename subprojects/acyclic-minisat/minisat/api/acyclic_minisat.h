#ifndef MINISAT_ACYCLIC_MINISAT_H
#define MINISAT_ACYCLIC_MINISAT_H

#include <utility>
#include <vector>

namespace Minisat {

using Edge = std::pair<int, int>;
using KnownGraph = std::vector<Edge>;
using EdgeSet = std::vector<Edge>;
using Constraint = std::pair<EdgeSet, EdgeSet>;
using Constraints = std::vector<Constraint>;

// precondition: known_graph and constraints must be remapped
bool am_solve(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints);

} // namespace Minisat

#endif // MINISAT_ACYCLIC_MINISAT_H