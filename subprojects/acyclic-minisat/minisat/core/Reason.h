#ifndef MINISAT_REASON_H
#define MINISAT_REASON_H

#include <set>
#include <functional>
#include <string>

namespace Minisat {

class Reason {
public:
  std::set<int> vars; // v1 & v2 & ... & vn
  Reason(): vars({}) {}
  Reason(const int var): vars({}) { vars.insert(var); }
  Reason(const std::set<int> &_vars): vars(_vars) {}
  Reason(const Reason &reason, const int &v): vars(reason.vars) { vars.insert(v); }
  Reason(const Reason &reason1, const Reason &reason2): vars(reason1.vars) { vars.insert(reason2.vars.begin(), reason2.vars.end()); }

  bool any() { return !vars.empty(); }

  bool operator == (const Reason &rhs) const {
    if (rhs.vars.size() != vars.size()) return false;
    for (auto i = vars.begin(), j = rhs.vars.begin(); i != vars.end() && j != rhs.vars.end(); i++, j++) {
      if (*i != *j) return false;
    }
    return true;
  }

  std::string to_string() const {
    if (vars.empty()) return "()";
    std::string s = "(";
    for (const auto &v : vars) s += std::to_string(v) + ", ";
    s += ')';
    return s;
  }

}; // class Reason

} // namespace Minisat

namespace std {
  template<>
  struct hash<Minisat::Reason> {
    size_t operator()(const Minisat::Reason &reason) const {
      int res = 0;
      for (auto v : reason.vars) {
        res ^= hash<int>()(v);
      }
      return res;
    }
  };
} // namespace std

#endif // MINISAT_REASON_H
