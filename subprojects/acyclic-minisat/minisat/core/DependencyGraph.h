#ifndef MINISAT_DEPENDENCY_GRAPH_H
#define MINISAT_DEPENDENCY_GRAPH_H

#include <vector>
#include <unordered_set>
#include <utility>

namespace Minisat {

constexpr static auto hash_pair = [](const auto &t) {
  auto &[t1, t2] = t;
  std::hash<int> h;
  return h(t1) ^ h(t2); 
};

struct DependencyGraph {
  int n;
  std::vector<std::unordered_set<std::pair<int, int>, decltype(hash_pair)>> pred, succ;

  DependencyGraph() {}

  void init(int _n) {
    n = _n;
    pred.assign(n, std::unordered_set<std::pair<int, int>, decltype(hash_pair)>());
    succ.assign(n, std::unordered_set<std::pair<int, int>, decltype(hash_pair)>());
  }

  void add_edge(int from, int to, int var) {
    succ.at(from).insert(std::make_pair(to, var));
    pred.at(to).insert(std::make_pair(from, var));
  } 

  bool contains_edge(int from, int to, int var) {
    bool flag = succ.at(from).contains(std::make_pair(to, var));
    assert(flag == pred.at(to).contains(std::make_pair(from, var)));
    return flag;
  }

  void remove_edge(int from, int to, int var) {
    if (succ.at(from).contains(std::make_pair(to, var))) {
      succ.at(from).erase(std::make_pair(to, var));
    }
    if (pred.at(to).contains(std::make_pair(from, var))) {
      pred.at(to).erase(std::make_pair(from, var));
    }
  }
}; // struct DependencyGraph

} // namespace Minisat

#endif // MINISAT_DEPENDENCY_GRAPH_H