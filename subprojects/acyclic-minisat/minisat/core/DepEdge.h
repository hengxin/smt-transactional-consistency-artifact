#ifndef MINISAT_DEPEDGE_H
#define MINISAT_DEPEDGE_H

namespace Minisat {

struct DepEdge {
  int from, to;
  EdgeType type;
};

} // namespace Minisat

#endif // MINISAT_DEPEDGE_H 