#ifndef MINISAT_LOGGER_H
#define MINISAT_LOGGER_H

#include <iostream>
#include <string>
#include <sstream>
#include <cassert>
#include <vector>
#include <set>

#include "minisat/core/OptOption.h"
#include "minisat/core/SolverTypes.h"

namespace Minisat::Logger {

auto log(std::string str, std::string end = "\n") -> bool {

#ifdef LOGGER_ENABLED
  std::cout << str << end;
  return true;
#else
  return false;
#endif

}

auto type2str(int type) -> std::string {
  std::string ret = "";
  switch (type) {
    case 0: ret = "SO"; break;
    case 1: ret = "WW"; break;
    case 2: ret = "WR"; break;
    case 3: ret = "RW"; break;
    default: assert(false);
  } 
  return ret;
}

template<typename T>
typename std::enable_if<std::is_same<T, int>::value || std::is_same<T, int64_t>::value, std::string>::type 
vector2str(const std::vector<T> &vec) {
  if (vec.empty()) return std::string{""};
  auto os = std::ostringstream{};
  for (auto i : vec) os << std::to_string(i) << ", ";
  auto ret = os.str();
  ret.erase(ret.length() - 2); // delete last ", "
  return ret; 
}

template<typename T>
typename std::enable_if<std::is_same<T, int>::value || std::is_same<T, int64_t>::value, std::string>::type 
set2str(const std::set<T> &s) {
  if (s.empty()) return std::string{""};
  auto os = std::ostringstream{};
  for (auto i : s) os << std::to_string(i) << ", ";
  auto ret = os.str();
  ret.erase(ret.length() - 2); // delete last ", "
  return ret; 
}

auto lits2str(vec<Lit> &lits) -> std::string {
  auto os = std::ostringstream{};
  for (int i = 0; i < lits.size(); i++) {
    const Lit &l = lits[i];
    int x = l.x >> 1;
    if (l.x & 1) os << "-";
    os << std::to_string(x);
    if (i + 1 != lits.size()) os << ", ";
  }
  return os.str(); 
}


} // namespace Minisat::Logger

#endif // MINISAT_LOGGER_H