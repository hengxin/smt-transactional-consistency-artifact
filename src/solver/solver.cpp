#include "solver.h"

#include <z3++.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"

using checker::history::Constraint;
using std::get;
using std::pair;
using std::reduce;
using std::unordered_map;
using std::vector;
using std::ranges::range;
using std::ranges::views::transform;
using z3::expr;

static constexpr auto hash_txn_pair = [](const pair<int64_t, int64_t> &p) {
  std::hash<int64_t> h;
  return h(p.first) ^ h(p.second);
};

static constexpr auto hash_z3_expr = [](const expr &e) { return e.hash(); };

using EdgeVarMap =
    unordered_map<pair<int64_t, int64_t>, expr, decltype(hash_txn_pair)>;

namespace checker::solver {

Solver::Solver(const history::DependencyGraph &known_graph,
               const std::vector<history::Constraint> &constraints)
    : solver{context} {
  EdgeVarMap edge_vars;

  auto get_var = [&, n = 0]() mutable {
    return context.bool_const(std::to_string(n++).c_str());
  };
  auto get_edge_var = [&](int64_t from, int64_t to) mutable {
    if (auto it = edge_vars.find({from, to}); it != edge_vars.end()) {
      return it->second;
    } else {
      return edge_vars.try_emplace({from, to}, get_var()).first->second;
    }
  };
  auto edge_to_var = transform([&](const Constraint::Edge &e) {
    const auto &[from, to, _] = e;
    return get_edge_var(from, to);
  });
  auto negate = [&](range auto r) {
    return r | transform([](const expr &e) { return !e; });
  };
  auto and_all = [&](range auto r) {
    return reduce(std::ranges::begin(r), std::ranges::end(r),
                  context.bool_val(true), std::bit_and{});
  };

  for (const auto &[from, to, _] : known_graph.edges()) {
    solver.add(get_edge_var(from, to) == true);
  }

  for (const auto &c : constraints) {
    auto either_vars = c.either_edges | edge_to_var;
    auto or_vars = c.or_edges | edge_to_var;
    solver.add((and_all(negate(either_vars)) & and_all(or_vars)) |
               (and_all(either_vars) & and_all(negate(or_vars))));
  }

  user_propagator =
      std::make_unique<DependencyGraphHasNoCycle>(solver, edge_vars);
}

bool Solver::solve() { return solver.check() == z3::sat; }

Solver::~Solver() = default;

struct DependencyGraphHasNoCycle : z3::user_propagator_base {
  unordered_map<z3::expr, pair<int64_t, int64_t>, decltype(hash_z3_expr)>
      var_edge;
  vector<size_t> fixed_vars_num_at_scope{0};
  vector<pair<z3::expr, bool>> fixed_vars;

  DependencyGraphHasNoCycle(z3::solver &solver, const EdgeVarMap &vars)
      : z3::user_propagator_base(&solver) {
    for (const auto &[p, e] : vars) {
      var_edge.try_emplace(e, p);
    }
  }

  void push() override {
    fixed_vars_num_at_scope.emplace_back(fixed_vars.size());
  }

  void pop(unsigned int num_scopes) override {
    fixed_vars_num_at_scope.resize(fixed_vars_num_at_scope.size() - num_scopes);
    fixed_vars.erase(fixed_vars.begin() + fixed_vars_num_at_scope.back(),
                     fixed_vars.end());
  }

  void fixed(const expr &var, const expr &value) override {
    fixed_vars.emplace_back(var, value.bool_value());
  }

  z3::user_propagator_base *fresh(z3::context &ctx) override { return this; }

  void final() override { throw std::runtime_error{"unimplemented"}; }
};

}  // namespace checker::solver
