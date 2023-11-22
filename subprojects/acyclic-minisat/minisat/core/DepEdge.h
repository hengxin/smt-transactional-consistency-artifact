#ifndef MINISAT_DEPEDGE_H
#define MINISAT_DEPEDGE_H

#include <minisat/core/EdgeType.h>

namespace Minisat {
struct DepEdge {
  int from, to;
  EdgeType type;
};

} // namespace Minisat

#endif // MINISAT_DEPEDGE_H 