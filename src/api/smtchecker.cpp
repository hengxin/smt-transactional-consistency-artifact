//
// Created by draco on 1/25/24.
//
#include "smtchecker.h"
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

bool verify(const char *filepath, const char *log_level, bool pruning, const char *solver_type, const char *history_type, bool perf, const char* perf_path) {
    std::string log_level_str{log_level};
    std::string history_type_str{history_type};
    auto log_level_map =
    std::unordered_map<std::string, boost::log::trivial::severity_level>{
            {"INFO", boost::log::trivial::info},
            {"WARNING", boost::log::trivial::warning},
            {"ERROR", boost::log::trivial::error},
            {"DEBUG", boost::log::trivial::debug},
            {"TRACE", boost::log::trivial::trace},
            {"FATAL", boost::log::trivial::fatal},
    };

    std::ranges::transform(log_level_str, log_level_str.begin(), toupper);
    if (log_level_map.contains(log_level_str)) {
        boost::log::core::get()->set_filter(boost::log::trivial::severity >=
                                            log_level_map.at(log_level_str));
    } else {
        std::ostringstream os;
        os << "Invalid log level '" << log_level_str << "'";
        throw std::invalid_argument{os.str()};
    }

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

    const auto all_history_types = std::set<std::string>{"cobra", "dbcop", "text", "elle"};
    if (all_history_types.contains(history_type_str)) {
        BOOST_LOG_TRIVIAL(debug)
                << "history type: "
                << history_type_str;
    } else {
        std::ostringstream os;
        os << "Invalid history type '" << history_type_str << "'";
        os << "All valid history types: 'dpcop' or 'cobra' or 'text' or 'elle'";
        throw std::invalid_argument{os.str()};
    }

    auto time = chrono::steady_clock::now();
    std::map<std::string, chrono::milliseconds> profile;
    history::History history;
    if (history_type_str == "dbcop") {
        // read history
        auto history_file = std::ifstream{filepath};
        if (!history_file.is_open()) {
            std::ostringstream os;
            os << "Cannot open file '" << filepath << "'";
            throw std::runtime_error{os.str()};
        }

        history = history::parse_dbcop_history(history_file);
    } else if (history_type_str == "cobra") {
        auto history_dir = filepath;
        try {
            history = history::parse_cobra_history(history_dir);
        } catch (const std::runtime_error &e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
    } else if (history_type_str == "text") {
        auto history_file = std::ifstream{filepath};
        if (!history_file.is_open()) {
            std::ostringstream os;
            os << "Cannot open file '" << filepath << "'";
            throw std::runtime_error{os.str()};
        }

        history = history::parse_text_history(history_file);
    } else if (history_type_str == "elle") {
        auto history_file = std::ifstream{filepath};
        if (!history_file.is_open()) {
            std::ostringstream os;
            os << "Cannot open file '" << filepath << "'";
            throw std::runtime_error{os.str()};
        }

        history = history::parse_text_history(history_file);
    } else {
        assert(0);
    }

    // compute known graph (WR edges) and constraints from history
    auto dependency_graph = history::known_graph_of(history);
    auto constraints = history::constraints_of(history, dependency_graph.wr);

    if (history_type_str == "elle") {
      BOOST_LOG_TRIVIAL(info)
          << "load known WW edges from '" << filepath << "_ww'";
      auto ww_file_path = std::string(filepath) + "_ww";
      auto ww_file = std::ifstream{ww_file_path};
      auto known_ww_edges = history::parse_elle_ww(ww_file);
      history::add_known_ww(dependency_graph, known_ww_edges);
    }

    // std::cout << dependency_graph << std::endl;

    auto display_constraints = [](const std::vector<history::Constraint> &constraints, std::string info = {""}) -> void {
        if (info != "") std::cout << info << std::endl;
        for (auto constraint : constraints) {
            std::cout << constraint << std::endl;
        }
        std::cout << std::endl;
    };
    // display_constraints(constraints, "Constraints before Pruning:");

    {
        auto curr_time = chrono::steady_clock::now();
        BOOST_LOG_TRIVIAL(info)
                << "construct time: "
                << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        profile["Construction"] = chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
    }

    auto accept = true;

    if (pruning == true) {
        accept = solver::prune_constraints(dependency_graph, constraints);

        // display_constraints(constraints, "Constraints after Pruning:");

        {
            auto curr_time = chrono::steady_clock::now();
            BOOST_LOG_TRIVIAL(info)
                    << "prune time: "
                    << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
            profile["Pruning"] = chrono::duration_cast<chrono::milliseconds>(curr_time - time);
            time = curr_time;
        }
    }

    if (accept) {
        // encode constraints and known graph
        auto solver = solver::SolverFactory::getSolverFactory().make(solver_type, dependency_graph, constraints);

        {
            auto curr_time = chrono::steady_clock::now();
            BOOST_LOG_TRIVIAL(info)
                    << "solver initializing time: "
                    << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
            profile["Encoding"] = chrono::duration_cast<chrono::milliseconds>(curr_time - time);
            time = curr_time;
        }

        // use SMT solver to solve constraints
        accept = solver->solve();

        {
            auto curr_time = chrono::steady_clock::now();
            BOOST_LOG_TRIVIAL(info)
                    << "solve time: "
                    << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
            profile["Solving"] = chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        }
        delete solver;
    }

    if (perf) {
      auto profile_map_to_json = [](const std::map<std::string, chrono::milliseconds> &map) -> std::string {
        std::stringstream ss;
        ss << "{";
        bool first = true;
        for (auto &&[k, v] : map) {
          if (!first) {
            ss << ",";
          }
          ss << "\"" << k << "\": " << v.count();
          first = false;
        }
        ss << "}" << std::endl;
        return ss.str();
      };
      auto json = profile_map_to_json(profile);

      BOOST_LOG_TRIVIAL(info) << json << std::endl;

      std::ofstream perf_file(perf_path);
      if (!perf_file.is_open()) {
        BOOST_LOG_TRIVIAL(error) << "failed to open perf file: " << perf_path;
      } else {
        perf_file << json;
        perf_file.close();
        BOOST_LOG_TRIVIAL(info) << "dot file saved: " << perf_path;
      }
    }

    std::cout << "accept: " << std::boolalpha << accept << std::endl;
    return accept;
}