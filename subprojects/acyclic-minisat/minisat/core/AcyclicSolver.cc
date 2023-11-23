#include "AcyclicSolver.h"

#include <iostream>

#include "minisat/core/OptOption.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/utils/Monitor.h"

namespace Minisat {

void AcyclicSolver::add_atom(int var) { atom_trail.push(var); }

void AcyclicSolver::newDecisionLevel() {
  trail_lim.push(trail.size());
  propagated_lits_trail_lim.push(propagated_lits_trail.size());
  atom_trail_lim.push(atom_trail.size());
}

CRef AcyclicSolver::propagate() {
  CRef confl = CRef_Undef;
  int num_props = 0;

  std::vector<int> vars_to_add;

  while (qhead < trail.size()) {
    Lit p = trail[qhead++]; // 'p' is the enqueued fact to propagate.
    vec<Watcher> &ws = watches.lookup(p);
    Watcher *i, *j, *end;
    num_props++;

#ifdef MONITOR_ENABLED
    Monitor::get_monitor()->extend_times++;
#endif

    // ---addon begin---
    if (value(var(p)) == l_True) {
      int v = var(p);
      if (!added_var[v]) { // TODO: this condition may be useless
        vars_to_add.push_back(v);
      }
    }
//     if (value(var(p)) == l_True) {
//       int v = var(p);
//       if (!added_var[v]) {

// #ifdef MONITOR_ENABLED
//         Monitor::get_monitor()->add_edge_times++;
// #endif
//         bool cycle = !solver_helper->add_edges_of_var(v);
//         if (cycle) {

// #ifdef MONITOR_ENABLED
//           Monitor::get_monitor()->find_cycle_times++;
// #endif
//           auto &conflict_clause = solver_helper->conflict_clauses.back();
//           vec<Lit> clause;
//           for (Lit l : conflict_clause) clause.push(~l);

// #ifdef MONITOR_ENABLED
//           Monitor::get_monitor()->cycle_edge_count_sum += clause.size();
// #endif

//           // std::cerr << "Adding: " << v << "\n";
//           // for (Lit l : conflict_clause) std::cerr << var(l) << "\n";
//           // std::cerr << "\n";

//           confl = ca.alloc(clause, false); 
//           solver_helper->conflict_clauses.pop_back();
//         } else {
//           added_var[v] = true;
//           add_atom(v);

//           auto &propagated_lits = solver_helper->propagated_lits;
//           for (Lit l : propagated_lits) {
//             if (value(l) == l_Undef) {
//               propagated_lits_trail.push(l);
//               // uncheckedEnqueue(l);
//             }
//           } 
//           propagated_lits.clear();
//         }
//       } 
//     }

    // ---addon end---

    for (i = j = (Watcher *)ws, end = i + ws.size(); i != end;) {
      // Try to avoid inspecting the clause:
      Lit blocker = i->blocker;
      if (value(blocker) == l_True) {
        *j++ = *i++;
        continue;
      }

      // Make sure the false literal is data[1]:
      CRef cr = i->cref;
      Clause &c = ca[cr];
      Lit false_lit = ~p;
      if (c[0] == false_lit)
        c[0] = c[1], c[1] = false_lit;
      assert(c[1] == false_lit);
      i++;

      // If 0th watch is true, then clause is already satisfied.
      Lit first = c[0];
      Watcher w = Watcher(cr, first);
      if (first != blocker && value(first) == l_True) {
        *j++ = w;
        continue;
      }

      // Look for new watch:
      for (int k = 2; k < c.size(); k++)
        if (value(c[k]) != l_False) {
          c[1] = c[k];
          c[k] = false_lit;
          watches[~c[1]].push(w);
          goto NextClause;
        }

      // Did not find watch -- clause is unit under assignment:
      *j++ = w;
      if (value(first) == l_False) {
        confl = cr;
        qhead = trail.size();
        // Copy the remaining watches:
        while (i < end)
          *j++ = *i++;
      }
      else
        uncheckedEnqueue(first, cr);

    NextClause:;
    }
    ws.shrink(i - j);
  }

  // ---- addon begin ----
  bool re_propagate = false;
  if (confl == CRef_Undef) {
    bool cycle = false;
    for (const auto &v : vars_to_add) {
      cycle = !solver_helper->add_edges_of_var(v);
      if (cycle) {

#ifdef MONITOR_ENABLED
        Monitor::get_monitor()->find_cycle_times++;
#endif

        auto &conflict_clause = solver_helper->conflict_clauses.back();
        vec<Lit> clause;
        for (Lit l : conflict_clause) clause.push(~l);

#ifdef MONITOR_ENABLED
        Monitor::get_monitor()->cycle_edge_count_sum += clause.size();
#endif

        confl = ca.alloc(clause, false); 
        solver_helper->conflict_clauses.pop_back();
      } else {
        added_var[v] = true;
        add_atom(v);

//         auto &propagated_lits = solver_helper->propagated_lits;
//         for (const auto &[lit, reason_lits] : propagated_lits) {
//           if (value(lit) == l_Undef) {
//             re_propagate = true;
//             vec<Lit> learnt_clause;
//             learnt_clause.push(lit); // lit is always false
//             for (const auto &l : reason_lits) {
//               if (l != lit) learnt_clause.push(l);
//             }
//             CRef cr = ca.alloc(learnt_clause, true);
//             uncheckedEnqueue(lit, cr);

// #ifdef MONITOR_ENABLED
//             Monitor::get_monitor()->propagated_lit_times++;
// #endif
//           }
//         }
        // propagated_lits.clear();
      }
    }
  }
  // ---- addon end ----

  propagations += num_props;
  simpDB_props -= num_props;

  if (re_propagate) {
    assert(confl == CRef_Undef);
    return propagate();
  }

  return confl;
}

void AcyclicSolver::cancelUntil(int level) {
  if (decisionLevel() > level) {
    for (int c = trail.size() - 1; c >= trail_lim[level]; c--) {
      Var x = var(trail[c]);
      assigns[x] = l_Undef;
      if (phase_saving > 1 || ((phase_saving == 1) && c > trail_lim.last()))
        polarity[x] = sign(trail[c]);
      insertVarOrder(x);
    }

    trail.shrink(trail.size() - trail_lim[level]);
    trail_lim.shrink(trail_lim.size() - level);

    // ---addon begin---
    for (int c = atom_trail.size() - 1; c >= atom_trail_lim[level]; c--) {
      auto atom = atom_trail[c]; // var
      added_var[atom] = false;
      solver_helper->remove_edges_of_var(atom);
    }
    atom_trail.shrink(atom_trail.size() - atom_trail_lim[level]);
    atom_trail_lim.shrink(atom_trail_lim.size() - level);
    // ---addon end---
  }
}

lbool AcyclicSolver::search(int nof_conflicts) { 
  assert(ok);
  int         backtrack_level;
  int         conflictC = 0;
  vec<Lit>    learnt_clause;
  starts++;

  for (; ; ) {
    int qhead_backup = qhead;
    CRef confl = propagate();
    if (confl != CRef_Undef) {
      // CONFLICT
      conflicts++; conflictC++;
      if (decisionLevel() == 0) { return l_False; } 

      learnt_clause.clear();
      analyze(confl, learnt_clause, backtrack_level);
      cancelUntil(backtrack_level);

      if (learnt_clause.size() == 1) {
        uncheckedEnqueue(learnt_clause[0]);
      } else {
        CRef cr = ca.alloc(learnt_clause, true);
        learnts.push(cr);
        attachClause(cr);
        claBumpActivity(ca[cr]);
        uncheckedEnqueue(learnt_clause[0], cr);
      }

      varDecayActivity();
      claDecayActivity();

      if (--learntsize_adjust_cnt == 0) {
        learntsize_adjust_confl *= learntsize_adjust_inc;
        learntsize_adjust_cnt    = (int)learntsize_adjust_confl;
        max_learnts             *= learntsize_inc;

        if (verbosity >= 1)
          printf("| %9d | %7d %8d %8d | %8d %8d %6.0f | %6.3f %% |\n", 
            (int)conflicts, 
            (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]), nClauses(), (int)clauses_literals, 
          (int)max_learnts, nLearnts(), (double)learnts_literals / nLearnts(), progressEstimate() * 100);
      }

    } else {
      // NO CONFLICT
      if ((nof_conflicts >= 0 && conflictC >= nof_conflicts) || !withinBudget()){
        // Reached bound on number of conflicts:
        progress_estimate = progressEstimate();
        cancelUntil(0);
        return l_Undef; 
      }

      // Simplify the set of problem clauses:
      if (decisionLevel() == 0 && !simplify())
        return l_False;

      if (learnts.size() - nAssigns() >= max_learnts)
        // Reduce the set of learnt clauses:
        reduceDB();

      Lit next = lit_Undef;
      // assumptions was cleared before entering solve(), so the following `while` will never be executed
      while (decisionLevel() < assumptions.size()) {
        // Perform user provided assumption:
        Lit p = assumptions[decisionLevel()];
        if (value(p) == l_True) {
          // Dummy decision level:
          newDecisionLevel();
        } else if (value(p) == l_False) {
          analyzeFinal(~p, conflict);
          return l_False;
        } else {
          next = p;
          break;
        }
      }

      // pick a new variable, go a step on search tree
      if (next == lit_Undef){
        // New variable decision:
        decisions++;
        if (next == lit_Undef) { next = pickBranchLit(); }
        if (next == lit_Undef) return l_True; // Model found
      }

      // Increase decision level and enqueue 'next'
      newDecisionLevel();
      uncheckedEnqueue(next);
    }
  }
}

AcyclicSolver::AcyclicSolver() {
  Solver();
  solver_helper = nullptr;
}

void AcyclicSolver::init(AcyclicSolverHelper *_solver_helper) { solver_helper = _solver_helper; }

static double luby(double y, int x) {
  // Find the finite subsequence that contains index 'x', and the
  // size of that subsequence:
    int size, seq;
    for (size = 1, seq = 0; size < x+1; seq++, size = 2*size+1);

    while (size-1 != x){
        size = (size-1)>>1;
        seq--;
        x = x % size;
    }

    return pow(y, seq);
}

lbool AcyclicSolver::solve_() { // same as Solver.cc
  model.clear();
    conflict.clear();
    if (!ok) return l_False;

    solves++;

    max_learnts = nClauses() * learntsize_factor;
    if (max_learnts < min_learnts_lim)
        max_learnts = min_learnts_lim;

    learntsize_adjust_confl   = learntsize_adjust_start_confl;
    learntsize_adjust_cnt     = (int)learntsize_adjust_confl;
    lbool   status            = l_Undef;

    if (verbosity >= 1){
        printf("============================[ Search Statistics ]==============================\n");
        printf("| Conflicts |          ORIGINAL         |          LEARNT          | Progress |\n");
        printf("|           |    Vars  Clauses Literals |    Limit  Clauses Lit/Cl |          |\n");
        printf("===============================================================================\n");
    }

    // Search:
    int curr_restarts = 0;
    while (status == l_Undef){
        double rest_base = luby_restart ? luby(restart_inc, curr_restarts) : pow(restart_inc, curr_restarts);
        status = search(rest_base * restart_first);
        if (!withinBudget()) break;
        curr_restarts++;
    }

    if (verbosity >= 1)
        printf("===============================================================================\n");


    if (status == l_True){
        // Extend & copy model:
        model.growTo(nVars());
        for (int i = 0; i < nVars(); i++) model[i] = value(i);
    }else if (status == l_False && conflict.size() == 0)
        ok = false;

    cancelUntil(0);
    return status;
}

Lit AcyclicSolver::pickBranchLit() {
    // choose next var and make a search decision, if a satisfiable model is found, return l_Undef 
    Var next = var_Undef;

    // Select the var who represents max / min edges
#ifdef SELECT_MAX_EDGES
    if (next == var_Undef) next = solver_helper->get_var_represents_max_edges();
#endif

#ifdef SELECT_MIN_EDGES
    if (next == var_Undef) next = solver_helper->get_var_represents_max_edges();
#endif

    // Random decision:
    if (next == var_Undef && drand(random_seed) < random_var_freq && !order_heap.empty()) {
        next = order_heap[irand(random_seed,order_heap.size())];
        if (value(next) == l_Undef && decision[next])
            rnd_decisions++;
    }

    // Activity based decision:
    while (next == var_Undef || value(next) != l_Undef || !decision[next]) {
        if (order_heap.empty()){
            next = var_Undef;
            break;
        } else {
            next = order_heap.removeMin();
        }
    }

    // Choose polarity based on different polarity modes (global or per-variable):
    if (next == var_Undef)
        return lit_Undef;
    else if (user_pol[next] != l_Undef)
        return mkLit(next, user_pol[next] == l_True);
    else if (rnd_pol) // false
        return mkLit(next, drand(random_seed) < 0.5);
    else
        return mkLit(next, polarity[next]);
}

bool AcyclicSolver::solve() {
  budgetOff();
  assumptions.clear();
  return solve_() == l_True;
}

AcyclicSolver::~AcyclicSolver() { delete solver_helper; }

} // namespace Minisat
