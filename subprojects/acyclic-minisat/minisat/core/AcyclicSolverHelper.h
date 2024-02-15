#ifndef MINISAT_ACYCLICSOLVER_HELPER
#define MINISAT_ACYCLICSOLVER_HELPER

#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <sstream>
#include <utility>

#include "minisat/core/Polygraph.h"
#include "minisat/core/ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"

// manage polygraph and ICDGraph

namespace Minisat {

constexpr static auto pair_hash_endpoint = [](const auto &t) {
  auto &[t1, t2] = t;
  std::hash<int64_t> h;
  return h(int64_t(t1)) ^ h(t2); 
};

class AcyclicSolverHelper {
  Polygraph *polygraph;
  ICDGraph icd_graph;
  std::set<std::pair<int, int>> vars_heap;
  std::unordered_map<int, std::unordered_map<int, std::set<int64_t>>> ww_keys; // (from, to) -> { keys }
  std::vector<std::unordered_set<int>> ww_to;
  std::vector<std::unordered_set<std::pair<int, int64_t>, decltype(pair_hash_endpoint)>> wr_to; // <to, key>

public:
  std::vector<std::vector<Lit>> conflict_clauses;
  std::vector<std::pair<Lit, std::vector<Lit>>> propagated_lits; // <lit, reason>

  AcyclicSolverHelper(Polygraph *_polygraph);
  void add_var(int var);
  void remove_var(int var);
  bool add_edges_of_var(int var);
  void remove_edges_of_var(int var);
  Var get_var_represents_max_edges();
  Var get_var_represents_min_edges();

  // getter
  Polygraph *get_polygraph();

}; // class AcyclicSolverHelper

namespace Logger {

auto wr_to2str(const std::unordered_set<std::pair<int, int64_t>, decltype(pair_hash_endpoint)> &wr_to_of_from) -> std::string;

}; // namespace Minisat::Logger

}
#endif // MINISAT_ACYCLICSOLVER_HELPER