#ifndef MINISAT_CONSTRCUTOR_H
#define MINISAT_CONSTRCUTOR_H

#include <cassert>
#include <algorithm>

#include <fmt/format.h>

#include "minisat/core/Polygraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/core/Graph.h"
#include "minisat/core/Solver.h"
#include "minisat/core/AcyclicSolver.h"
#include "minisat/core/PairConflict.h"
#include "minisat/core/OptOption.h"
#include "minisat/core/Logger.h"

namespace Minisat {

Polygraph *construct(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints, AcyclicSolver &solver, std::vector<Lit> &unit_lits, 
                     const std::unordered_map<int, std::unordered_set<int64_t>> &write_keys_of) {
  Polygraph *polygraph = new Polygraph(n_vertices); // unused n_vars

  Logger::log("[Known Graph]");
  Logger::log(fmt::format("n = {}", n_vertices));
  for (const auto &[type, from, to, keys] : known_graph) {
    polygraph->add_known_edge(from, to, type, keys);
    Logger::log(fmt::format("{}: {} -> {}, keys = {}", Logger::type2str(type), from, to, Logger::vector2str(keys)));
  }
  polygraph->write_keys_of = write_keys_of;

  Logger::log("[Constraints]");
  assert(unit_lits.empty());
  int var_count = 0;
  const auto &wr_cons = constraints;

  Logger::log("[1. WR Constraints]");
  for (const auto &[read, writes, key] : wr_cons) {
    vec<Lit> lits;
    auto cons = std::vector<int>{};
    for (const auto &write : writes) {
      solver.newVar();
      int v = var_count++; // WR(k)
      polygraph->map_wr_var(v, write, read, key);
      lits.push(mkLit(v));
      cons.emplace_back(v);
    }
    if (lits.size() != 1) {
      solver.addClause_(lits); // v1 | v2 | ... | vn
      int index = polygraph->add_wr_cons(cons);
      for (const auto &v : cons) {
        polygraph->map_wr_cons(v, index);
      }

      #ifdef ENCODE_WR_UNIQUE
        for (unsigned i = 0; i < cons.size(); i++) {
          for (unsigned j = i + 1; j < cons.size(); j++) {
            int v1 = cons[i], v2 = cons[j];
            vec<Lit> tmp_lits;
            tmp_lits.push(~mkLit(v1)), tmp_lits.push(~mkLit(v2));
            solver.addClause_(tmp_lits);
          }
        }
      #endif
    } else {
      unit_lits.emplace_back(lits[0]);
    }
    Logger::log(Logger::lits2str(lits));
  }

  polygraph->set_n_vars(var_count);

  Logger::log("[Var to Theory Interpretion]");
  for (int v = 0; v < var_count; v++) {
    if (polygraph->is_ww_var(v)) {
      const auto &[from, to, keys] = polygraph->ww_info[v];
      Logger::log(fmt::format("{}: WW, {} -> {}, keys = {}", v, from, to, Logger::set2str(keys)));
    } else if (polygraph->is_wr_var(v)) {
      const auto &[from, to, key] = polygraph->wr_info[v];
      Logger::log(fmt::format("{}: WR({}), {} -> {}", v, key, from, to));
    } else if (polygraph->is_rw_var(v)) {
      const auto &[from, to] = polygraph->rw_info[v];
      Logger::log(fmt::format("{}: RW, {} -> {}", v, from, to));
    } else {
      assert(false);
    }
  }

  return polygraph;
}

} // namespace Minisat

#endif // MINISAT_CONSTRCUTOR_H