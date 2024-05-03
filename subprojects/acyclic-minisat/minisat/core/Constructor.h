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
                     int suggest_distance,
                     const std::unordered_map<int, std::unordered_map<int64_t, int>> &write_steps, 
                     const std::unordered_map<int, std::unordered_map<int64_t, int>> &read_steps) {
  Polygraph *polygraph = new Polygraph(n_vertices); // unused n_vars

  Logger::log("[Known Graph]");
  Logger::log(fmt::format("n = {}", n_vertices));
  for (const auto &[type, from, to, keys] : known_graph) {
    polygraph->add_known_edge(from, to, type, keys);
    Logger::log(fmt::format("{}: {} -> {}, keys = {}", Logger::type2str(type), from, to, Logger::vector2str(keys)));
    if (type == 4) { // LO
      polygraph->is_observer[from] = polygraph->is_observer[to] = true;
      polygraph->lo_edges.emplace(from, to);
    }
  }

  Logger::log("[Constraints]");
  assert(unit_lits.empty());
  int var_count = 0;
  const auto &[ww_cons, wr_cons] = constraints;
  Logger::log("[1. WW Constraints]");

  auto suggest_ww = [&suggest_distance, &write_steps](int v1, int v2, int either_, int or_, const std::vector<int64_t> &keys) -> int {
    // v1: either -> or; v2: or -> either
    assert(suggest_distance != -1);
    // TODO: ww heuristics
    int max_diff = 0; // either_steps - or_steps
    int64_t sum_diff = 0;
    for (const auto &key : keys) {
      int either_steps = write_steps.at(either_).at(key);
      int or_steps = write_steps.at(or_).at(key);
      sum_diff += either_steps - or_steps;
      if (std::abs(either_steps - or_steps) > std::abs(max_diff)) max_diff = either_steps - or_steps;
    }
    if (std::abs(max_diff) <= suggest_distance) return -1;
    return max_diff > 0 ? v2 : v1;
    // return sum_diff > 0 ? v2 : v1;
  };

  int suggest_ww_cnt = 0;
  for (const auto &[either_, or_, keys] : ww_cons) {
    solver.newVar(), solver.newVar();
    int v1 = var_count++, v2 = var_count++;

    auto keys_set = std::set(keys.begin(), keys.end());
    
    polygraph->map_ww_var(v1, either_, or_, keys_set);
    polygraph->map_ww_var(v2, or_, either_, keys_set);

    vec<Lit> lits; // v1 + v2 = 1 => (v1 | v2) & ((~v1) | (~v2))
    lits.push(mkLit(v1)), lits.push(mkLit(v2));
    solver.addClause_(lits); // (v1 | v2)
    Logger::log(Logger::lits2str(lits));
    lits.clear(), lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
    solver.addClause_(lits); // ((~v1) | (~v2))

    if (suggest_distance != -1) {
      int suggest_v = suggest_ww(v1, v2, either_, or_, keys);
      if (suggest_v != -1) {
        ++suggest_ww_cnt;
        unit_lits.emplace_back(mkLit(suggest_v));
      } 
    } 
  }

  Logger::log(fmt::format("suggest ww count = {}", suggest_ww_cnt));
  std::cout << "suggest ww cnt = " << suggest_ww_cnt << std::endl;

  Logger::log("[2. WR Constraints]");

  auto suggest_wr = [&suggest_distance, &write_steps, &read_steps](const std::vector<int> &cons, int read, const std::vector<int> &writes, int64_t key) -> int {
    // TODO: wr heuristics
    return -1;
  };

  for (const auto &[read, writes, key] : wr_cons) {
    vec<Lit> lits;
    auto cons = std::vector<int>{};
    for (const auto &write : writes) {
      solver.newVar();
      int v = var_count++; // WR(k)
      polygraph->map_wr_var(v, write, read, key);
      lits.push(mkLit(v));
      cons.emplace_back(v);
      if (polygraph->is_observer.contains(read)) {
        polygraph->observer_wr_candidates[read].emplace_back(v);
      }
    }
    if (lits.size() != 1) {
      solver.addClause_(lits); // v1 | v2 | ... | vn
      int index = polygraph->add_wr_cons(cons);
      for (const auto &v : cons) {
        polygraph->map_wr_cons(v, index);
      }
      if (suggest_distance != -1) {
        int suggest_v = suggest_wr(cons, read, writes, key);
        if (suggest_v != -1) unit_lits.emplace_back(mkLit(suggest_v));
      }
    } else {
      unit_lits.emplace_back(lits[0]);
    }
    // TODO: in fact, v1 + v2 + ... + vn = 1,
    //       here we only consider the v1 + v2 + ... + vn >= 1 half,
    //       another part which may introduce great power of unit propagate is to be considered
    Logger::log(Logger::lits2str(lits));
  }

#ifdef HARD_CODING_LO_WW
  for (const auto &[from, to] : polygraph->lo_edges) {
    for (const auto &wr_v1 : polygraph->observer_wr_candidates.at(from)) {
      const auto &[v1_from, v1_to, v1_keys] = polygraph->ww_info.at(wr_v1);
      for (const auto &wr_v2 : polygraph->observer_wr_candidates.at(to)) {
        const auto &[v2_from, v2_to, v2_key] = polygraph->ww_info.at(wr_v2);
        if (v1_from == v2_from) {
          // wr_v1 & wr_v2 => false(v1_from -> v2_from, selfloop directly leads to conflict)
          vec<Lit> lits;
          lits.push(~mkLit(wr_v1)), lits.push(~mkLit(wr_v2)); // (~wr_v1) || (~wr_v2)
          solver.addClause_(lits);
        } else {
          assert(polygraph->ww_var_of.contains(v1_from) && polygraph->ww_var_of[v1_from].contains(v2_from));
          int ww_v = polygraph->ww_var_of[v1_from][v2_from];
          // wr_v1 & wr_v2 => ww_v
          vec<Lit> lits;
          lits.push(~mkLit(wr_v1)), lits.push(~mkLit(wr_v2)), lits.push(~mkLit(ww_v));
          solver.addClause_(lits);
        }
      }
    }
  }
#endif

  polygraph->set_n_vars(var_count);

  Logger::log("[Var to Theory Interpretion]");
  for (int v = 0; v < var_count; v++) {
    if (polygraph->is_ww_var(v)) {
      const auto &[from, to, keys] = polygraph->ww_info[v];
      Logger::log(fmt::format("{}: WW, {} -> {}, keys = {}", v, from, to, Logger::set2str(keys)));
    } else if (polygraph->is_wr_var(v)) {
      const auto &[from, to, key] = polygraph->wr_info[v];
      Logger::log(fmt::format("{}: WR({}), {} -> {}", v, key, from, to));
    } else {
      assert(false);
    }
  }

  return polygraph;
}

// Polygraph *construct(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints, Solver &solver) {
//   int n_constraints = constraints.size();
//   Polygraph *polygraph = new Polygraph(n_vertices, n_constraints * 2);
//   for (const auto &[from, to] : known_graph) {
//     polygraph->add_known_edge(from, to);
//   }
//   int var_count = 0;
//   for (const auto &[either_, or_] : constraints) {
//     auto add_constraint = [&](const EdgeSet &edges) -> void {
//       for (const auto &[from, to] : edges) {
//         polygraph->add_constraint_edge(var_count, from, to);
//       }
//       ++var_count;
//     };
//     add_constraint(either_), add_constraint(or_);
//   }
//   assert(var_count == n_constraints * 2);
//   for (int i = 0; i < var_count; i += 2) {
//     solver.newVar(), solver.newVar();
//     int v1 = i, v2 = i + 1;
//     vec<Lit> lits;
//     lits.push(mkLit(v1)), lits.push(mkLit(v2));
//     solver.addClause_(lits);
//     lits.clear(), lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
//     solver.addClause_(lits);
//   }

// #ifdef INIT_PAIR_CONFLICT
//   init_pair_conflict(polygraph, solver);
// #endif

//   return polygraph;
// }

} // namespace Minisat

#endif // MINISAT_CONSTRCUTOR_H