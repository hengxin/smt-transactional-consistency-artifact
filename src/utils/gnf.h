#ifndef CHECKER_UTILS_GNF_H
#define CHECKER_UTILS_GNF_H 

#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace fs = std::filesystem;

namespace checker::utils {
void write_to_gnf_file(fs::path &gnf_path, 
                      const history::DependencyGraph &known_graph,
                      const history::Constraints &constraints) {
  // TODO: write_to_gnf_file
  // using GNFEdge = std::pair<int64_t, int64_t>;
  // constexpr auto hash_gnf_edge_endpoint = [](const auto &t) {
  //   auto [t1, t2] = t;
  //   std::hash<int64_t> h;
  //   return h(t1) ^ h(t2);
  // };
  // std::unordered_map<GNFEdge, int64_t, decltype(hash_gnf_edge_endpoint)> edge_id;
  // std::unordered_map<int64_t, GNFEdge> id_edge;
  // std::unordered_map<int64_t, int64_t> node_id;
  // int64_t n_var = 0, n_node = 0;
  // auto alloc_edge_id = [&](const auto &edge) mutable {
  //   if (edge_id.contains(edge)) return;
  //   edge_id[edge] = ++n_var;
  //   id_edge[n_var] = edge;
  // };
  // auto alloc_node_id = [&](const auto &node) mutable {
  //   if (node_id.contains(node)) return;
  //   node_id[node] = n_node++;
  // };
  // auto alloc_edge = [&](const auto &from, const auto &to) {
  //   alloc_edge_id((GNFEdge) {from, to});
  //   alloc_node_id(from), alloc_node_id(to);
  // };
  // int64_t n_clauses = 0;
  // for (const auto &[from, to, _] : known_graph.edges()) {
  //   alloc_edge(from, to);
  //   n_clauses++;
  // } 
  // for (const auto &cons : constraints) {
  //   n_clauses += 4; // (A | B) & (~A | ~B) & (A | ~a1 | ~a2 | ... | ~an) & (B | ~b1 | ~b2 | ... | ~bn) 
  //   for (const auto &[from, to, _] : cons.either_edges) n_clauses++, alloc_edge(from, to); // (~A | a1) & (~A | a2) & ... & (~A | an)
  //   for (const auto &[from, to, _] : cons.or_edges) n_clauses++, alloc_edge(from, to); // (~B | b1) & (~B | b2) & ... & (~B | bn)
  // }
  // int64_t n_edge = n_var, n_edge_set = n_edge;
  // n_var += constraints.size() * 2;

  // std::ofstream ofs;
  // ofs.open(gnf_path, std::ios::out | std::ios::trunc);
  // ofs << "p cnf " << n_var + 1 << " " << n_clauses << std::endl; // n_var + 1 represents the acyclicity
  // for (const auto &[from, to, _] : known_graph.edges()) {
  //   ofs << edge_id[(GNFEdge) {from, to}] << ' ' << 0 << std::endl;
  // }
  
  // for (const auto &cons : constraints) {
  //   int64_t a = ++n_edge_set, b = ++n_edge_set;
  //   ofs << a << " " << b << " " << 0 << std::endl;   // A | B
  //   ofs << -a << " " << -b << " " << 0 << std::endl; // ~A | ~B
  //   ofs << a;
  //   for (const auto &[from, to, _] : cons.either_edges) {
  //     ofs << " " << -edge_id[(GNFEdge) {from, to}];
  //   }
  //   ofs << " " << 0 << std::endl; // A | ~a1 | ~a2 | ... | ~an
  //   ofs << b;
  //   for (const auto &[from, to, _] : cons.or_edges) {
  //     ofs << " " << -edge_id[(GNFEdge) {from, to}];
  //   }
  //   ofs << " " << 0 << std::endl; // B | ~b1 | ~b2 | ... | ~bn
  //   for (const auto &[from, to, _] : cons.either_edges) {
  //     ofs << -a << " " << edge_id[(GNFEdge) {from, to}] << " " << 0 << std::endl;
  //     // (~A | a1) & (~A | a2) & ... & (~A | an)
  //   }
  //   for (const auto &[from, to, _] : cons.or_edges) {
  //     ofs << -b << " " << edge_id[(GNFEdge) {from, to}] << " " << 0 << std::endl;
  //     // (~B | b1) & (~B | b2) & ... & (~B | bn)
  //   }
  // }
  // ofs << n_var + 1 << " " << 0 << std::endl;

  // // describe the graph
  // ofs << "digraph int " << n_node << " " << n_edge << " " << 0 << std::endl;
  // for (int64_t id = 1; id <= n_edge; id++) {
  //   auto &[from, to] = id_edge[id];
  //   ofs << "edge" << " " << 0 << " " << node_id[from] << " " << node_id[to] << " " << id << std::endl;
  // }
  // ofs << "acyclic" << " " << 0 << " " << n_var + 1 << std::endl;

  // ofs.close();
}
} // namespace

#endif // CHECKER_UTILS_GNF_H