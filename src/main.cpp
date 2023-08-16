#include <z3++.h>

#include <argparse/argparse.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <cctype>
#include <chrono>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <syncstream>
#include <unordered_map>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "history/history.h"
#include "solver/pruner.h"
#include "solver/solverFactory.h"
#include "utils/log.h"

namespace history = checker::history;
namespace solver = checker::solver;
namespace chrono = std::chrono;

auto main(int argc, char **argv) -> int {
  // handle cmdline args, see checker --help
  auto args = argparse::ArgumentParser{"checker", "0.0.1"};
  args.add_argument("history").help("History file");
  args.add_argument("--log-level")
      .help("Logging level")
      .default_value(std::string{"INFO"});
  args.add_argument("--pruning")
      .help("Do pruning")
      .default_value(false)
      .implicit_value(true);
  args.add_argument("--solver")
      .help("Select backend solver")
      .default_value(std::string{"z3"});

  try {
    args.parse_args(argc, argv);
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  auto log_level = args.get("--log-level");
  auto log_level_map =
      std::unordered_map<std::string, boost::log::trivial::severity_level>{
          {"INFO", boost::log::trivial::info},
          {"WARNING", boost::log::trivial::warning},
          {"ERROR", boost::log::trivial::error},
          {"DEBUG", boost::log::trivial::debug},
          {"TRACE", boost::log::trivial::trace},
          {"FATAL", boost::log::trivial::fatal},
      };

  std::ranges::transform(log_level, log_level.begin(), toupper);
  if (log_level_map.contains(log_level)) {
    boost::log::core::get()->set_filter(boost::log::trivial::severity >=
                                        log_level_map.at(log_level));
  } else {
    std::ostringstream os;
    os << "Invalid log level '" << log_level << "'";
    throw std::invalid_argument{os.str()};
  }

  auto solver_type = args.get("--solver");
  const auto all_solvers = std::set<std::string>{"z3", "monosat", "acyclic-minisat"};
  if (all_solvers.contains(solver_type)) {
    BOOST_LOG_TRIVIAL(debug)
        << "use "
        << solver_type
        << " as backend solver";
  } else {
    std::ostringstream os;
    os << "Invalid solver '" << solver_type << "'";
    os << "All valid solvers: 'z3' or 'monosat' or 'acyclic-minisat'";
    throw std::invalid_argument{os.str()};
  }

  auto time = chrono::steady_clock::now();

  // read history
  auto history_file = std::ifstream{args.get("history")};
  if (!history_file.is_open()) {
    std::ostringstream os;
    os << "Cannot open file '" << args.get("history") << "'";
    throw std::runtime_error{os.str()};
  }

  auto history = history::parse_dbcop_history(history_file);

  // compute known graph (WR edges) and constraints from history
  auto dependency_graph = history::known_graph_of(history);
  auto constraints = history::constraints_of(history, dependency_graph.wr);

  {
    auto curr_time = chrono::steady_clock::now();
    BOOST_LOG_TRIVIAL(info)
        << "construct time: "
        << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
    time = curr_time;
  }

  CHECKER_LOG_COND(trace, logger) {
    logger << "history: " << history << "\ndependency graph:\n"
           << dependency_graph;

    for (const auto &c : constraints) {
      logger << c;
    }
  }

  auto accept = true;

  if (args["--pruning"] == true) {
    accept = solver::prune_constraints(dependency_graph, constraints);

    {
      auto curr_time = chrono::steady_clock::now();
      BOOST_LOG_TRIVIAL(debug)
          << "prune time: "
          << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
      time = curr_time;
    }
  }

  if (accept) {
    // encode constraints and known graph
    auto solver = solver::SolverFactory::getSolverFactory().make(solver_type, dependency_graph, constraints);

    // use SMT solver to solve constraints
    accept = solver->solve();

    {
      auto curr_time = chrono::steady_clock::now();
      BOOST_LOG_TRIVIAL(info)
          << "solve time: "
          << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
    }

    delete solver;
  }
  std::cout << "accept: " << std::boolalpha << accept << std::endl;

  return 0;
}
