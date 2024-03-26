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
#include "api/smtchecker.h"

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
  args.add_argument("--history-type")
      .help("History type")
      .default_value(std::string{"dbcop"});
  args.add_argument("--perf")
      .help("Perform performance analysis")
      .default_value(true);
  args.add_argument("--perf-path")
      .help("Performance analysis output path")
      .default_value(std::string{"./perf.json"});

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

    verify(args.get("history").c_str(), args.get("--log-level").c_str(), args["--pruning"] == true,
           args.get("--solver").c_str(), args.get("--history-type").c_str(),
           args["--perf"] == true, args.get("--perf-path").c_str());
  return 0;
}
