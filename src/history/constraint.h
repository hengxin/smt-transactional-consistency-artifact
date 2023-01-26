#ifndef CHECKER_HISTORY_CONSTRAINT_H
#define CHECKER_HISTORY_CONSTRAINT_H

#include <cstdint>
#include <iosfwd>
#include <tuple>
#include <vector>

#include "dependencygraph.h"
#include "history.h"

namespace checker::history {

struct Constraint {
  using Edge = std::tuple<int64_t, int64_t, EdgeInfo>;

  int64_t either_txn_id;
  int64_t or_txn_id;
  std::vector<Edge> either_edges;
  std::vector<Edge> or_edges;

  friend auto operator<<(std::ostream &os, const Constraint &constraint)
      -> std::ostream &;
};

auto constraints_of(const History &history, const DependencyGraph::SubGraph &wr)
    -> std::vector<Constraint>;

}  // namespace checker::history

#endif /* CHECKER_HISTORY_CONSTRAINT_H */
