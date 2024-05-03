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
      .default_value(std::string{"none"});
  args.add_argument("--solver")
      .help("Select backend solver")
      .default_value(std::string{"acyclic-minisat"});
  args.add_argument("--history-type")
      .help("History type")
      .default_value(std::string{"dbcop"});
  args.add_argument("--isolation-level")
      .help("Target isolation level")
      .default_value(std::string{"ser"});

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

  auto isolation_level = args.get("--isolation-level");
  const auto all_isolation_levels = std::set<std::string>{"ser", "si"};
  if (all_isolation_levels.contains(isolation_level)) {
    BOOST_LOG_TRIVIAL(debug)
      << "target isolation level: "
      << isolation_level;
  } else {
    std::ostringstream os;
    os << "Unknown isolation level '" << isolation_level << "'";
    os << "Supported isolation levels: 'ser'(serializabe) or 'si'(snapshot isolation)";
    throw std::invalid_argument{os.str()};
  }

  auto history_type = args.get("--history-type");
  const auto all_history_types = std::set<std::string>{"cobra", "dbcop", "elle-list-append"};
  if (all_history_types.contains(history_type)) {
    BOOST_LOG_TRIVIAL(debug) << "history type: " << history_type;
  } else {
    std::ostringstream os;
    os << "Invalid history type '" << history_type << "'";
    os << "All valid history types: 'dpcop' or 'cobra' or 'elle-list-append'";
    throw std::invalid_argument{os.str()};
  }

  if (history_type != "elle-list-append") {
    throw std::runtime_error{"This instance only supports list append!"};
  }

  auto time = chrono::steady_clock::now();

  history::History history;
  auto known_ww = std::vector<std::tuple<int64_t, int64_t, int64_t>>{};
  if (history_type == "dbcop") {
    // read history
    throw std::runtime_error{"Not Implemented"};
    // auto history_file = std::ifstream{args.get("history")};
    // if (!history_file.is_open()) {
    //   std::ostringstream os;
    //   os << "Cannot open file '" << args.get("history") << "'";
    //   throw std::runtime_error{os.str()};
    // }

    // history = history::parse_dbcop_history(history_file);
  } else if (history_type == "cobra") {
    throw std::runtime_error{"Not Implemented"};
    // auto history_dir = args.get("history");
    // try {
    //   history = history::parse_cobra_history(history_dir);
    // } catch (const std::runtime_error &e) {
    //   std::cerr << e.what() << std::endl;
    //   return 1;
    // }
  } else if (history_type == "elle-list-append") {
    auto history_file = std::ifstream{args.get("history")};
    if (!history_file.is_open()) {
      std::ostringstream os;
      os << "Cannot open file '" << args.get("history") << "'";
      throw std::runtime_error{os.str()};
    }

    history = history::parse_elle_list_append_history(history_file);
  } else {
    assert(0);
  }

  CHECKER_LOG_COND(trace, logger) {
    logger << "history: " << history;
  }

  if (!check_single_write(history)) { // violates SingleWrite Constraint
    BOOST_LOG_TRIVIAL(debug) << "check single write: failed";
    BOOST_LOG_TRIVIAL(debug) << "This history violates SingleWrite constraint";
    throw std::runtime_error{"Detect multiple writes on 1 key in a transaction. This mode is not supported yet."};
  }
  BOOST_LOG_TRIVIAL(debug) << "check single write: ok";

  if (!check_list_prefix(history)) { // violates LIST-PREFIX
    BOOST_LOG_TRIVIAL(debug) << "check LIST-PREFIX: failed";
    auto accept = false;
    std::cout << "accept: " << std::boolalpha << accept << std::endl;
    return 0;
  }
  BOOST_LOG_TRIVIAL(debug) << "check LIST-PREFIX: ok";

  auto ins_history = history::instrumented_history_of(history);

  CHECKER_LOG_COND(trace, logger) {
    logger << "instrumented history: " << ins_history;
  }

  // TODO: history meta info
  auto history_meta_info = history::compute_history_meta_info(history);
  
  // compute known graph (WR edges) and constraints from history
  auto dependency_graph = history::known_graph_of(ins_history);

  CHECKER_LOG_COND(trace, logger) {
    logger << "dependency graph:\n"
           << dependency_graph;
  }

  auto constraints = std::pair<std::vector<history::WWConstraint>, std::vector<history::WRConstraint>>{};
  try {
    constraints = history::constraints_of(ins_history); 
  } catch (std::runtime_error &e) {
    std::cerr << e.what() << std::endl;
    auto accept = false;
    std::cout << "accept: " << std::boolalpha << accept << std::endl;
    return 0;
  }

  {
    auto curr_time = chrono::steady_clock::now();
    BOOST_LOG_TRIVIAL(info)
        << "construct time: "
        << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
    time = curr_time;
  }

  CHECKER_LOG_COND(trace, logger) {
    // logger << "history: " << history << "\ndependency graph:\n"
    //        << dependency_graph;

    logger << "constraints\n";
    logger << "ww: \n";
    const auto &[ww_constraints, wr_constraints] = constraints;
    for (const auto &c : ww_constraints) { logger << c; }
    logger << "wr: \n";
    for (const auto &c : wr_constraints) { logger << c; }
  }

  auto accept = true;

  if (args.get("--pruning") != "none") {
    auto pruning_method = args.get("--pruning");
    BOOST_LOG_TRIVIAL(debug) << "pruning method: " << pruning_method;
    
    auto pruned = true;
    if (pruning_method == "normal") {
      if (isolation_level == "ser") {
        accept = solver::prune_constraints(dependency_graph, constraints);
      } else if (isolation_level == "si") {
        accept = solver::prune_si_constraints(dependency_graph, constraints); // hard encode, bad implementation!
      }
    } else if (pruning_method == "fast") {
      // must use fast pruning
      if (isolation_level == "ser") {
        accept = solver::fast_prune_constraints(dependency_graph, constraints);
      } else if (isolation_level == "si") {
        accept = solver::fast_prune_si_constraints(dependency_graph, constraints); // hard encode, bad implementation!
      }
    } else {
      pruned = false;
      BOOST_LOG_TRIVIAL(info) << "unknown pruning method \"" 
                              << pruning_method
                              << "\", expect in {\"normal\", \"fast\", \"none\"}, skip pruning";
    }

    // display_constraints(constraints, "Constraints after Pruning:");

    if (pruned) {
      auto curr_time = chrono::steady_clock::now();
      BOOST_LOG_TRIVIAL(info)
          << "prune time: "
          << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
      time = curr_time;
    }
  }

  if (accept) {
    // encode constraints and known graph
    auto solver = solver::SolverFactory::getSolverFactory().make(solver_type, 
                                                                 dependency_graph, 
                                                                 constraints, 
                                                                 history_meta_info,
                                                                 isolation_level);

    {
      auto curr_time = chrono::steady_clock::now();
      BOOST_LOG_TRIVIAL(info)
          << "solver initializing time: "
          << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
      time = curr_time;
    }

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
