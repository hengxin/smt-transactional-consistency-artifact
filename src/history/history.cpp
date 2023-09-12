#include "history.h"

#include <bits/ranges_algo.h>

#include <boost/endian.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>
#include <istream>
#include <iterator>
#include <numeric>
#include <ostream>
#include <ranges>
#include <string>
#include <syncstream>
#include <unordered_set>
#include <utility>
#include <vector>

#include "utils/to_container.h"

using std::ranges::count_if;
using std::ranges::views::iota;
using std::ranges::views::transform;

static auto read_int64(std::istream &in) -> int64_t {
  int64_t n;
  in.read(reinterpret_cast<char *>(&n), sizeof(n));
  boost::endian::little_to_native_inplace(n);
  return n;
}

static auto read_str(std::istream &in) -> std::string {
  auto size = read_int64(in);
  auto s = std::string(size, '\0');
  in.read(s.data(), size);
  return s;
}

static auto read_bool(std::istream &in) -> bool { return in.get() != 0; }

namespace checker::history {

auto parse_dbcop_history(std::istream &is) -> History {
  constexpr int64_t init_session_id = 0;
  constexpr int64_t init_txn_id = 0;

  auto history = History{};
  int64_t current_session_id = 1;
  int64_t current_txn_id = 1;
  auto keys = std::unordered_set<int64_t>{};

  auto parse_event = [&](Transaction &txn) {
    auto is_write = read_bool(is);
    auto key = read_int64(is);
    auto value = read_int64(is);
    auto success = read_bool(is);

    if (success) {
      keys.insert(key);
      txn.events.emplace_back(Event{
          .key = key,
          .value = value,
          .type = is_write ? EventType::WRITE : EventType::READ,
          .transaction_id = txn.id,
      });
    }
  };

  auto parse_txn = [&](Session &session) {
    auto &txn = session.transactions.emplace_back(Transaction{
        .id = current_txn_id++,
        .session_id = session.id,
    });

    auto size = read_int64(is);
    for ([[maybe_unused]] auto i : iota(0, size)) {
      parse_event(txn);
    }

    auto success = read_bool(is);
    if (!success) {
      session.transactions.pop_back();
    }
  };

  auto parse_session = [&] {
    auto &session =
        history.sessions.emplace_back(Session{.id = current_session_id++});

    auto size = read_int64(is);
    for ([[maybe_unused]] auto i : iota(0, size)) {
      parse_txn(session);
    }
  };

  [[maybe_unused]] auto id = read_int64(is);
  [[maybe_unused]] auto session_num = read_int64(is);
  [[maybe_unused]] auto key_num = read_int64(is);
  [[maybe_unused]] auto txn_num = read_int64(is);
  [[maybe_unused]] auto event_num = read_int64(is);
  [[maybe_unused]] auto info = read_str(is);
  [[maybe_unused]] auto start = read_str(is);
  [[maybe_unused]] auto end = read_str(is);

  auto size = read_int64(is);
  for ([[maybe_unused]] auto i : iota(0, size)) {
    parse_session();
  }

  history.sessions.emplace_back(Session{
      .id = init_session_id,
      .transactions = std::vector{Transaction{
          .id = init_txn_id,
          .events = keys  //
                    | transform([](auto key) {
                        return Event{
                            .key = key,
                            .value = 0,
                            .type = EventType::WRITE,
                            .transaction_id = init_txn_id,
                        };
                      })  //
                    | utils::to<std::vector<Event>>,
          .session_id = init_session_id,
      }},
  });

  auto count_all = [](auto &&) { return true; };
  BOOST_LOG_TRIVIAL(info) << "#sessions: " << history.sessions.size();
  BOOST_LOG_TRIVIAL(info) << "#transactions: "
                          << count_if(history.transactions(), count_all);
  BOOST_LOG_TRIVIAL(info) << "#events: "
                          << count_if(history.events(), count_all);

  return history;
}

auto operator<<(std::ostream &os, const History &history) -> std::ostream & {
  auto out = std::osyncstream{os};

  for (const auto &session : history.sessions) {
    out << "\nSession " << session.id << ":\n";

    for (const auto &txn : session.transactions) {
      out << "Txn " << txn.id << ": ";

      for (const auto &event : txn.events) {
        out << (event.type == EventType::READ ? "R(" : "W(") << event.key
            << ", " << event.value << "), ";
      }

      out << "\n";
    }
  }

  return os;
}

auto n_rw_same_key_txns_of(History &history) -> int {
  int ret = 0;
  for (auto transaction : history.transactions()) {
    std::unordered_map<int64_t, bool> r_keys;
    for (auto [key, value, type, _] : transaction.events) {
      if (type == EventType::READ) {
        r_keys[key] = true;
      } else { // type == EventType::WRITE
        if (r_keys.contains(key)) {
          ++ret;
          break;
        }
      }
    }
  }
  return ret;
}

auto n_txns_of(History &history) -> int {
  int ret = 0;
  for (auto transaction : history.transactions()) ++ret;
  return ret;
}

auto n_written_key_txns_of(History &history) -> std::unordered_map<int64_t, int> {
  std::unordered_map<int64_t, std::unordered_set<int64_t>> written_txns_of_key;
  for (auto transaction : history.transactions()) {
    for (auto [key, value, type, txn_id] : transaction.events) {
      if (type != EventType::WRITE) continue;
      written_txns_of_key[key].insert(txn_id);
    }
  }
  std::unordered_map<int64_t, int> n_written_txns_of_key;
  for (const auto &[key, txns] : written_txns_of_key) {
    n_written_txns_of_key[key] = txns.size();
  }
  return n_written_txns_of_key;
}

}  // namespace checker::history
