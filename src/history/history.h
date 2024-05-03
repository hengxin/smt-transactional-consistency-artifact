#ifndef CHECKER_HISTORY_HISTORY_H
#define CHECKER_HISTORY_HISTORY_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <ranges>
#include <vector>
#include <unordered_map>

using std::unordered_map;
using std::vector;

namespace checker::history {

enum class EventType { READ, WRITE };

struct Event {
  int64_t id; // event id
  int64_t key;
  int64_t write_value;
  std::vector<int64_t> read_values;
  EventType type;
  int64_t transaction_id;
};

struct Transaction {
  int64_t id;
  std::vector<Event> events;
  int64_t session_id;
};

struct Session {
  int64_t id;
  std::vector<Transaction> transactions;
};

struct History {
  std::vector<Session> sessions;

  friend auto operator<<(std::ostream &os, const History &history)
      -> std::ostream &;

  std::ranges::range auto transactions() const {
    return sessions  //
           | std::ranges::views::transform([](const auto &s) {
               return std::ranges::views::all(s.transactions);
             })  //
           | std::ranges::views::join;
  }

  std::ranges::range auto events() const {
    return transactions()  //
           | std::ranges::views::transform([](const auto &txn) {
               return std::ranges::views::all(txn.events);
             })  //
           | std::ranges::views::join;
  }
};

struct HistoryMetaInfo {
  int n_sessions, n_total_transactions, n_total_events;
  int64_t n_nodes;
  unordered_map<int64_t, int64_t> begin_node, end_node; // txn_id -> node_id
  unordered_map<int64_t, int64_t> write_node; // event_id -> node_id for write event
  unordered_map<int64_t, unordered_map<int64_t, int64_t>> read_node; // event_id -> (index -> node_id) for read event
  unordered_map<int64_t, int64_t> event_value; // node_id -> value(write_value or a single read_value)
};

auto compute_history_meta_info(const History &history) -> HistoryMetaInfo;

/**
 * Read history from an input stream. The history is in dbcop format.
 *
 * Dbcop format:
 *
 * DBCOP_FILE := ID SESSION_NUM KEY_NUM TXN_NUM EVENT_NUM INFO START_TIME END_TIME
 * HISTORY := SIZE SESSION_1 ... SESSION_{SIZE}
 * SESSION := SIZE TRANSACTION_1 ... TRANSACTION_{SIZE}
 * TRANSACTION := SIZE EVENT_1 ... EVENT_{SIZE}
 * EVENT := IS_WRITE KEY VALUE SUCCESS
 * ID, SESSION_NUM, KEY_NUM, TXN_NUM, EVENT_NUM, SIZE, KEY, VALUE := int64_t
 * INFO, START, END := null terminated string
 * IS_WRITE, SUCCESS := bool
 */
auto parse_dbcop_history(std::istream &is) -> History;
auto parse_cobra_history(const std::string &history_dir) -> History;
auto parse_elle_list_append_history(std::ifstream &is) -> History;

auto n_txns_of(History &history) -> int;
auto n_rw_same_key_txns_of(History &history) -> int;
auto n_written_key_txns_of(History &history) -> std::unordered_map<int64_t, int>;

auto check_single_write(const History &history) -> bool;
auto check_list_prefix(const History &history) -> bool;

}  // namespace checker::history

namespace std {
template <>
struct hash<checker::history::Session> {
  auto operator()(const checker::history::Session &session) const {
    return hash<int64_t>{}(session.id);
  }
};

template <>
struct hash<checker::history::Transaction> {
  auto operator()(const checker::history::Transaction &txn) const {
    return hash<int64_t>{}(txn.id);
  }
};
}  // namespace std

#endif /* CHECKER_HISTORY_HISTORY_H */
