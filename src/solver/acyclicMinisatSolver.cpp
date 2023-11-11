#include <filesystem>
#include <cassert>
#include <unordered_map>
#include <iostream>

#include <acyclic_minisat.h>

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
                                           const history::Constraints &constraints) {
  std::cerr << "Not implemented!" << std::endl; // TODO: acyclic-minisat
  assert(0);
}

/*
AcyclicMinisatSolver::AcyclicMinisatSolver(const history::DependencyGraph &known_graph,
                                           const std::vector<history::Constraint> &constraints) {
  // ? previous version: write to .agnf file
  // agnf_path = fs::current_path();
  // agnf_path.append("acyclicminisat_tmp_input.agnf");
  // try {
  //   utils::write_to_agnf_file(agnf_path, known_graph, constraints);
  // } catch (std::runtime_error &e) {
  //   // TODO   
  // }

  int n = known_graph.num_vertices();
  n_vertices = n;
  // 0. simplify dependency_graph and constraints
  auto simplify = [&](const history::DependencyGraph &known_graph, 
                      const std::vector<history::Constraint> &constraints
                      ) -> std::pair<SimplifiedKnownGraph, SimplifiedConstraints> {
    int cur_node_id = 0;
    auto node_id = std::unordered_map<int64_t, int>{};
    auto get_node_id = [&](int64_t x) -> int {
      if (node_id.contains(x)) return node_id.at(x);
      return node_id[x] = cur_node_id++;
    };
    auto simplified_known_graph = SimplifiedKnownGraph{};
    for (const auto &[from, to, _] : known_graph.edges()) {
      simplified_known_graph.emplace_back(std::make_pair(get_node_id(from), get_node_id(to)));
    }
    auto simplified_constraints = SimplifiedConstraints{};
    for (const auto &constraint : constraints) {
      auto either_edges = SimplifiedEdgeSet{};
      for (const auto &[from, to, _] : constraint.either_edges) {
        either_edges.emplace_back(std::make_pair(get_node_id(from), get_node_id(to)));
      }
      auto or_edges = SimplifiedEdgeSet{};
      for (const auto &[from, to, _] : constraint.or_edges) {
        or_edges.emplace_back(std::make_pair(get_node_id(from), get_node_id(to)));
      }
      simplified_constraints.emplace_back(std::make_pair(either_edges, or_edges));
    } 
    return std::make_pair(simplified_known_graph, simplified_constraints);
  };
  auto [simplified_known_graph, simplified_constraints] = simplify(known_graph, constraints);

  // std::cerr << simplified_known_graph.size() << std::endl;
  // for (const auto &[from, to] : simplified_known_graph) {
  //   std::cerr << from << " " << to << std::endl;
  // }
  // std::cerr << simplified_constraints.size() << std::endl;
  // for (const auto &[either_, or_] : simplified_constraints) {
  //   for (const auto &[from, to] : either_) std::cerr << from << " " << to << std::endl;
  //   for (const auto &[from, to] : or_) std::cerr << from << " " << to << std::endl;
  // }

  // 1. remove bridges and devide into components(by scc)
  auto auxgraph = utils::TarjanGraph(n);
  //  1.1 construct auxiliary graph 
  for (const auto &[from, to] : simplified_known_graph) {
    auxgraph.add_edge(from, to);
  }
  for (const auto &[either_, or_] : simplified_constraints) { // the word 'or' is reserved
    for (const auto &[from, to] : either_) {
      auxgraph.add_edge(from, to);
    }
    for (const auto &[from, to] : or_) {
      auxgraph.add_edge(from, to);
    }
  }
  //  1.2 remove bridges
  // auto no_bridge_known_graph = SimplifiedKnownGraph{};
  // auto no_bridge_constraints = SimplifiedConstraints{};
  
  int n_removed_bridges = 0;
  for (const auto &[from, to] : simplified_known_graph) {
    if (auxgraph.is_bridge(from, to)) {
      ++n_removed_bridges;
      continue;
    }
    no_bridge_known_graph.emplace_back(std::make_pair(from, to));
  }
  for (const auto &[either_, or_] : simplified_constraints) {
    auto new_either = SimplifiedEdgeSet{};
    for (const auto &[from, to] : either_) {
      if (auxgraph.is_bridge(from, to)) {
        ++n_removed_bridges;
        continue;
      } 
      new_either.emplace_back(std::make_pair(from, to));
    }
    auto new_or = SimplifiedEdgeSet{};
    for (const auto &[from, to] : or_) {
      if (auxgraph.is_bridge(from, to)) {
        ++n_removed_bridges;
        continue;
      } 
      new_or.emplace_back(std::make_pair(from, to));
    }
    no_bridge_constraints.emplace_back(std::make_pair(new_either, new_or));
  }

  // std::cerr << no_bridge_known_graph.size() << std::endl;
  // std::cerr << no_bridge_constraints.size() << std::endl;

  BOOST_LOG_TRIVIAL(debug) << "#removed bridges: " << n_removed_bridges;
  
  //  1.3 divide into components(by scc), not applied here
  // int n_components = auxgraph.get_n_scc();
  // polygraph_components.assign(n_components, std::pair<SimplifiedKnownGraph, SimplifiedConstraints>());
  // for (const auto &[from, to] : no_bridge_known_graph) {
  //   assert(auxgraph.get_scc(from) == auxgraph.get_scc(to));
  //   int bel_scc = auxgraph.get_scc(from);
  //   auto &[sub_known_gragh, _] = polygraph_components.at(bel_scc);
  //   sub_known_gragh.emplace_back(std::make_pair(from, to));
  // }
  // for (const auto &[either_, or_] : no_bridge_constraints) {
  //   int bel_scc = auxgraph.get_scc(either_.at(0).first);
  //   // checking consistency, can be skipped
  //   auto check = [&](const SimplifiedEdgeSet &edges) -> bool {
  //     for (const auto &[from, to] : edges) {
  //       if (auxgraph.get_scc(from) != bel_scc || auxgraph.get_scc(to) != bel_scc) {
  //         return false;
  //       }
  //     }
  //     return true;
  //   };
  //   assert(check(either_) && check(or_));
  //   auto &[_, sub_constraints] = polygraph_components.at(bel_scc);
  //   sub_constraints.emplace_back(std::make_pair(either_, or_));
  // }

} */

auto AcyclicMinisatSolver::solve() -> bool {
  // TODO: call acyclic-minisat as backend solver
  // ? a toy implementation: call acyclic-minisat as a subprocess
  // auto obuf = subprocess::check_output({"/home/rikka/acyclic-minisat/build/minisat_core", agnf_path});
  // BOOST_LOG_TRIVIAL(debug) << "stdout of acyclic-minisat length: " << obuf.length;
  // std::string output = "";
  // for (char ch : obuf.buf) output += ch;
  // output = output.substr(0, output.length() - 1); // delete last '\n'
  // BOOST_LOG_TRIVIAL(debug) << "stdout of acyclic-minisat: " << output;
  // return output == "SAT";

  // auto remap = [](SimplifiedKnownGraph &known_graph, SimplifiedConstraints &constraints) -> int {
  //   std::unordered_map<int, int> node_id;
  //   int node_count = 0;
  //   auto get_node_id = [&](int x) -> int {
  //     if (!node_id.contains(x)) node_id[x] = node_count++;
  //     return node_id.at(x);
  //   };
  //   for (auto &[from, to] : known_graph) {
  //     from = get_node_id(from), to = get_node_id(to);
  //   }
  //   for (auto &[either_, or_] : constraints) {
  //     for (auto &[from, to] : either_) {
  //       from = get_node_id(from), to = get_node_id(to);
  //     }
  //     for (auto &[from, to] : or_) {
  //       from = get_node_id(from), to = get_node_id(to);
  //     }
  //   }
  //   return node_count;
  // };

  // bool accept = true;
  // for (auto &[known_graph, constraints] : polygraph_components) {
  //   if (constraints.empty()) continue;
  //   int n_vertices = remap(known_graph, constraints);
  //   BOOST_LOG_TRIVIAL(trace) 
  //     << "solving for component, " 
  //     << "#vertices: " << n_vertices << "; " 
  //     << "#constraints: " << constraints.size();
  //   accept &= Minisat::am_solve(n_vertices, known_graph, constraints);
  //   if (!accept) return false;
  // }
  // return accept; // always true here

  return Minisat::am_solve(n_vertices, no_bridge_known_graph, no_bridge_constraints);
}

AcyclicMinisatSolver::~AcyclicMinisatSolver() = default;

} // namespace checker::solver