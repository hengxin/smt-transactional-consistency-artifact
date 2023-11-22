#include <filesystem>
#include <cassert>
#include <unordered_map>
#include <iostream>

#include "acyclicMinisatSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/agnf.h"
#include "utils/subprocess.hpp"
#include "utils/tarjan_graph.h"

namespace fs = std::filesystem;

namespace checker::solver {

AcyclicMinisatSolver::AcyclicMinisatSolver(const history::DependencyGraph &known_graph,
                                           const std::vector<history::Constraint> &constraints) {
  auto get_minisat_edgetype = [](EdgeType t) -> int {
    switch (t) {
      case EdgeType::SO: return 0;
      case EdgeType::WW: return 1;
      case EdgeType::WR: return 2;
      case EdgeType::RW: return 3;
    }
    assert(0);
  };
  
  n_vertices = known_graph.num_vertices();
  for (const auto &[from, to, info] : known_graph.edges()) {
    simplified_known_graph.emplace_back(std::make_tuple(from, to, get_minisat_edgetype(info.get().type)));
  }
  for (const auto &constraint : constraints) {
    const auto &[_1, _2, either_edges, or_edges] = constraint;
    auto simplified_either_edges = SimplifiedEdgeSet{}, simplified_or_edges = SimplifiedEdgeSet{};
    auto add_simplified_edges = [&](SimplifiedEdgeSet &simplified_edges, const std::vector<history::Constraint::Edge> &edges) {
      for (const auto &[from, to, info] : edges) {
        simplified_edges.emplace_back(std::make_tuple(from, to, get_minisat_edgetype(info.type)));
      }
    };
    add_simplified_edges(simplified_either_edges, either_edges);
    add_simplified_edges(simplified_or_edges, or_edges);
  }
}

auto AcyclicMinisatSolver::solve() -> bool {
  return Minisat::am_solve(n_vertices, simplified_known_graph, simplified_constraints);
}

AcyclicMinisatSolver::~AcyclicMinisatSolver() = default;

} // namespace checker::solver