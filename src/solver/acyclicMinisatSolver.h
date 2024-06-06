#ifndef CHECKER_SOLVER_ACYCLICMINISATSOLVER_H
#define CHECKER_SOLVER_ACYCLICMINISATSOLVER_H

#include <vector>
#include <utility>
#include <filesystem>

#include "abstractSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace fs = std::filesystem;

namespace checker::solver {

struct AcyclicMinisatSolver : AbstractSolver {
  using AMEdge = std::tuple<int, int, int, std::vector<int64_t>>; // <type, from, to, keys>
                                                                  // see specific type info in "acyclic_minisat_types.h"
  using AMKnownGraph = std::vector<AMEdge>;

  using AMWRConstraint = std::tuple<int, std::vector<int>, int64_t>; // <read, write(s), key> 
  using AMConstraints = std::vector<AMWRConstraint>;

  std::string target_isolation_level;

  int n_vertices;
  AMKnownGraph am_known_graph;
  AMConstraints am_constraints;

  std::unordered_map<int, std::unordered_set<int64_t>> write_keys_of; // txn_id -> { key }
  
  AcyclicMinisatSolver(const history::DependencyGraph &known_graph,
                       const history::Constraints &constraints,
                       const history::HistoryMetaInfo &history_meta_info,
                       const std::string &isolation_level);

  auto solve() -> bool override;

  ~AcyclicMinisatSolver();
};

} // namespace checker::solver

#endif