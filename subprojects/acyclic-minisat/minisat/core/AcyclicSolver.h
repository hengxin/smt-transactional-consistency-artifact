#ifndef MINISAT_ACYCLICSOLVER_H
#define MINISAT_ACYCLICSOLVER_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "minisat/core/Solver.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/core/AcyclicSolverHelper.h"
#include "minisat/mtl/Vec.h"

namespace Minisat {
class AcyclicSolver : public Solver {
  vec<int> atom_trail;
  vec<int> atom_trail_lim;
  vec<Lit> propagated_lits_trail;
  vec<int> propagated_lits_trail_lim;
  std::unordered_map<int, bool> added_var;
  std::unordered_map<int, std::unordered_set<int>> conflict_vars_of;

  AcyclicSolverHelper *solver_helper;

protected:
  void add_atom(int var);
  void newDecisionLevel(); // override
  CRef propagate(); // override
  void cancelUntil(int level); // override
  Lit pickBranchLit(); // override

  lbool search(int nof_conflicts); // same as Solver.h

public:
  AcyclicSolver();
  void init(AcyclicSolverHelper *_solver_helper);
  lbool solve_(); // same as Solver.h
  bool solve();
  ~AcyclicSolver();
};
} // namespace Minisat

#endif // MINISAT_ACYCLICSOLVER_H