#ifndef MINISAT_ACYCLIC_MINISAT_H
#define MINISAT_ACYCLIC_MINISAT_H

#include <utility>
#include <vector>

namespace Minisat {

using Edge = std::tuple<int, int, int>; // <from, to, type>
// here type should be positioned as c++ enum, but a severe problem was encountered when acyclic-minisat was being compiled as a shared lib, so a bad implementation was used instead
// SO: 0
// WW: 1
// WR: 2
// RW: 3
using KnownGraph = std::vector<Edge>;
using EdgeSet = std::vector<Edge>;
using Constraint = std::pair<EdgeSet, EdgeSet>;
using Constraints = std::vector<Constraint>;

// precondition: known_graph and constraints must be remapped
bool am_solve(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints);

} // namespace Minisat

#endif // MINISAT_ACYCLIC_MINISAT_H