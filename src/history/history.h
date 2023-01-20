#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <vector>

namespace checker::history {

enum class EventType { READ, WRITE };

struct Event {
  int64_t key;
  int64_t value;
  EventType type;
};

struct Transaction {
  int64_t id;
  std::vector<Event> events;
};

struct Session {
  int64_t id;
  std::vector<Transaction> transactions;
};

struct History {
  std::vector<Session> sessions;

  friend std::ostream &operator<<(std::ostream &os, const History &history);

  static History parse_dbcop_history(std::istream &is);
};

}  // namespace checker::history

namespace std {
template <>
struct hash<checker::history::Session> {
  size_t operator()(const checker::history::Session &session) {
    return hash<int64_t>{}(session.id);
  }
};

template <>
struct hash<checker::history::Transaction> {
  size_t operator()(const checker::history::Transaction &txn) {
    return hash<int64_t>{}(txn.id);
  }
};
}  // namespace std
