#ifndef MINISAT_ACYCLIC_MINISAT_TYPES_H
#define MINISAT_ACYCLIC_MINISAT_TYPES_H

#include <utility>
#include <vector>
#include <cstdint>

namespace Minisat {

using Edge = std::tuple<int, int, int, std::vector<int64_t>>; // <type, from, to, keys>
                                                              // type = 0: SO, keys = {}
                                                              //        1: WR,
                                                              //        2: CO, keys = {}
using KnownGraph = std::vector<Edge>;

using WRConstraint = std::tuple<int, std::vector<int>, int64_t>; // <read, write(s), key> 
using Constraints = std::vector<WRConstraint>;

} // namespace Minisat

#endif // MINISAT_ACYCLIC_MINISAT_TYPES_H