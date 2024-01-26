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

extern std::vector<int64_t> conflict_cycle;

bool verify(const char *filepath, const char *log_level, bool pruning, const char *solver_type, const char *history_type, bool output_dot, const char* dot_path) {
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

        history = history::parse_elle_history(history_file);
    } else {
        assert(0);
    }

    // compute known graph (WR edges) and constraints from history
    auto dependency_graph = history::known_graph_of(history);
    auto constraints = history::constraints_of(history, dependency_graph.wr);

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

    if (pruning == true) {
        accept = solver::prune_constraints(dependency_graph, constraints);

        // display_constraints(constraints, "Constraints after Pruning:");

        {
            auto curr_time = chrono::steady_clock::now();
            BOOST_LOG_TRIVIAL(info)
                    << "prune time: "
                    << chrono::duration_cast<chrono::milliseconds>(curr_time - time);
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

        if (!accept && output_dot) {
            std::string dot = "digraph {\n";
            // nodes
            for (auto txn_id : conflict_cycle) {
                auto it = std::ranges::find_if(history.transactions().begin(), history.transactions().end(),
                                               [txn_id](const history::Transaction& txn) { return txn.id == txn_id; });
                if (it == history.transactions().end()) {
                    BOOST_LOG_TRIVIAL(error) << "txn_id not found: " << txn_id;
                }
                dot += "\"Transaction(id=" + std::to_string(txn_id) + ")\" [ops=\"";
                for (std::size_t i = 0; const auto& event : it->events) {
                    dot += "Operation(type=" + (event.type == history::EventType::READ ? std::string("READ") : std::string("WRITE"))  +
                           ", key=" + std::to_string(event.key) + ", value=" + std::to_string(event.value) +
                           ", transaction=Transaction(id=" + std::to_string(txn_id) + "), id=" + std::to_string(i++) + "), ";
                }
                dot.erase(dot.size() - 2);
                dot += "\"]\n";
            }

            // edges
            std::unordered_map<std::pair<int64_t, int64_t>, checker::history::EdgeInfo, decltype([](const std::pair<int64_t, int64_t> &p) {
                std::hash<int64_t> h;
                return h(p.first) ^ h(p.second);
            })> edge_map;
            for (const auto &[from, to, e] : dependency_graph.edges()) {
                edge_map[{from, to}] = e;
            }

            auto getEdgeType = [&](int64_t from, int64_t to) -> checker::history::EdgeInfo {
                if (edge_map.contains({from, to})) {
                    return edge_map[{from, to}];
                }
                for (auto& constraint : constraints) {
                    auto it = std::find_if(constraint.either_edges.begin(), constraint.either_edges.end(), [&](const auto& edge) { return std::get<0>(edge) == from && std::get<1>(edge) == to; });
                    if (it != constraint.either_edges.end()) {
                        for (auto& edge : constraint.either_edges) {
                            edge_map[{std::get<0>(edge), std::get<1>(edge)}] = std::get<2>(edge);
                        }
                    }
                }
                if (edge_map.contains({from, to})) {
                    return edge_map[{from, to}];
                } else {
                    BOOST_LOG_TRIVIAL(error) << "edge not found: " << from << " " << to;
                }
                return checker::history::EdgeInfo{};
            };

            conflict_cycle.push_back(conflict_cycle.front());
            for (auto it = conflict_cycle.begin(); it != conflict_cycle.end() && it + 1 != conflict_cycle.end(); ++it) {
                std::ostringstream oss;
                oss << getEdgeType(*it, *(it + 1));
                auto edge_info = oss.str();
                auto new_end = std::remove_if(edge_info.begin(), edge_info.end(), [](char c) { return c == '(' || c == ')'; });
                edge_info.erase(new_end, edge_info.end());
                dot += "\"Transaction(id=" + std::to_string(*it) + ")\" -> \"Transaction(id=" + std::to_string(*(it + 1)) + ")\" [label=\"" + edge_info + "\"]\n";
            }
            dot += "}";
            BOOST_LOG_TRIVIAL(info) << std::endl << "dot output:" << std::endl << dot << std::endl;
            std::ofstream dot_file(dot_path);
            if (!dot_file.is_open()) {
                BOOST_LOG_TRIVIAL(error) << "failed to open dot file: " << dot_path;
            } else {
                dot_file << dot;
                dot_file.close();
                BOOST_LOG_TRIVIAL(info) << "dot file saved: " << dot_path;
            }
        }

        delete solver;
    }
    std::cout << "accept: " << std::boolalpha << accept << std::endl;
    return accept;
}