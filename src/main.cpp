#include <z3++.h>

#include <argparse/argparse.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <cctype>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "history/history.h"
#include "solver/solver.h"
#include "utils/log.h"

namespace history = checker::history;
namespace solver = checker::solver;

auto main(int argc, char **argv) -> int {
  auto args = argparse::ArgumentParser{"checker", "0.0.1"};
  args.add_argument("history").help("History file");
  args.add_argument("--log-level")
      .help("Logging level")
      .default_value(std::string{"INFO"});

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

  auto history_file = std::ifstream{args.get("history")};
  if (!history_file.is_open()) {
    std::ostringstream os;
    os << "Cannot open file '" << args.get("history") << "'";
    throw std::runtime_error{os.str()};
  }

  auto history = history::parse_dbcop_history(history_file);
  auto dependency_graph = history::known_graph_of(history);
  auto constraints = history::constraints_of(history, dependency_graph.wr);

  CHECKER_LOG_COND(trace, logger) {
    logger << "history: " << history << '\n'
           << "RW:\n"
           << dependency_graph.rw << '\n'
           << "WW:\n"
           << dependency_graph.ww << '\n'
           << "SO:\n"
           << dependency_graph.so << '\n'
           << "WR:\n"
           << dependency_graph.wr;

    for (const auto &c : constraints) {
      logger << c;
    }
  }

  auto solver = solver::Solver{dependency_graph, constraints};
  std::cout << "accept: " << std::boolalpha << solver.solve() << std::endl;

  return 0;
}
