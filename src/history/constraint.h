#ifndef CHECKER_HISTORY_CONSTRAINT_H
#define CHECKER_HISTORY_CONSTRAINT_H

#include <cstdint>
#include <iosfwd>
#include <tuple>
#include <vector>
#include <string>
#include <set>

#include "dependencygraph.h"
#include "history.h"

namespace checker::history {

struct WRConstraint {
  using Edge = std::tuple<int64_t, int64_t, EdgeInfo>;

  int64_t key;
  int64_t read_txn_id;
  std::unordered_set<int64_t> write_txn_ids;

  friend auto operator<<(std::ostream &os, const WRConstraint &constraint)
      -> std::ostream &;
};

using Constraints = std::vector<WRConstraint>;

auto constraints_of(const History &history) -> Constraints;

auto measuring_repeat_values(const Constraints &constraints) -> void;

}  // namespace checker::history

#endif /* CHECKER_HISTORY_CONSTRAINT_H */
