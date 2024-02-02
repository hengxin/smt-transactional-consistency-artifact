#include "minisat/api/acyclic_minisat.h"
#include "minisat/core/Solver.h"
#include "minisat/core/Polygraph.h"
#include "minisat/core/AcyclicSolver.h"
#include "minisat/core/AcyclicSolverHelper.h"
#include "minisat/core/Constructor.h"
#include "minisat/core/Logger.h"

namespace Minisat {

bool am_solve(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints) {
  Logger::log("[Acyclic Minisat QxQ]");
  AcyclicSolver S;
  Polygraph *polygraph = construct(n_vertices, known_graph, constraints, S);
  AcyclicSolverHelper *solver_helper = nullptr;
  try {
    solver_helper = new AcyclicSolverHelper(polygraph);
  } catch (std::runtime_error &e) {
    return false; // UNSAT
  }
  S.init(solver_helper);
  if (!S.simplify()) return false; // UNSAT, decided by unit propagation
  bool accept = S.solve();
  delete polygraph;

#ifdef MONITOR_ENABLED
        Monitor::get_monitor()->show_statistics();
#endif

  return accept;
}

} // namespace Minisat