#include <filesystem>
#include <cassert>
#include <unordered_map>
#include <iostream>

#include <acyclic_minisat.h>
#include <acyclic_minisat_si.h>

#include "acyclicMinisatSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/agnf.h"
#include "utils/subprocess.hpp"
#include "utils/tarjan_graph.h"

namespace fs = std::filesystem;

using EdgeType = checker::history::EdgeType;

namespace checker::solver {

AcyclicMinisatSolver::AcyclicMinisatSolver(const history::DependencyGraph &known_graph,
                                           const history::Constraints &constraints,
                                           const history::HistoryMetaInfo &history_meta_info,
                                           const std::string &isolation_level) {
  assert(isolation_level == "tcc");

  // 0. touch n_vertices
  n_vertices = known_graph.num_vertices();

  auto node_id = std::unordered_map<int64_t, int>{};
  auto nodes_cnt = 0;
  auto remap = [&node_id, &nodes_cnt](int64_t x) -> int {
    if (node_id.contains(x)) return node_id.at(x);
    return node_id[x] = nodes_cnt++;
  };

  // 1. construct am_known_graph
  for (const auto &[from, to, info] : known_graph.edges()) {
    int t = -1;
    switch (info.get().type) {
      case EdgeType::SO : t = 0; break;
      case EdgeType::WR : t = 1; break;
      case EdgeType::CO : t = 2; break;
      default: assert(false);
    }
    assert(t != -1);
    am_known_graph.emplace_back(AMEdge{t, remap(from), remap(to), info.get().keys});
  }

  // 2. construct am_constraints(wr_constraints)
  const auto &wr_cons = constraints;
  auto &am_wr_cons = am_constraints;
  for (const auto &[key, read_txn_id, write_txn_ids] : wr_cons) {
    auto new_write_txn_ids = std::vector<int>();
    std::transform(write_txn_ids.begin(), write_txn_ids.end(),
                   std::back_inserter(new_write_txn_ids),
                   remap);
    am_wr_cons.emplace_back(AMWRConstraint{remap(read_txn_id), new_write_txn_ids, key});
  }

  CHECKER_LOG_COND(trace, logger) {
    logger << "node map: ";
    for (const auto &[k, v] : node_id) {
      logger << "(" << k << ", " << v << "), ";
    }
    logger << "\n";
  }

  // 3. copy history meta info
  for (const auto &[txn_id, keys] : history_meta_info.write_keys_of) {
    write_keys_of[remap(txn_id)] = keys; // copy keys
  }
}

auto AcyclicMinisatSolver::solve() -> bool {
  bool ret = true;
  assert(target_isolation_level == "tcc");
  ret = Minisat::am_solve(n_vertices, am_known_graph, am_constraints, write_keys_of);
  return ret;
}

AcyclicMinisatSolver::~AcyclicMinisatSolver() = default;

} // namespace checker::solver