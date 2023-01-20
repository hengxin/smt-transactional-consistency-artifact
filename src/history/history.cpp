#include "history.h"

#include <boost/endian.hpp>
#include <cstdint>
#include <istream>
#include <iterator>
#include <ostream>
#include <ranges>
#include <string>
#include <syncstream>
#include <unordered_set>
#include <utility>

using std::ssize;
using std::ranges::views::iota;
using std::ranges::views::transform;

static int64_t read_int64(std::istream &in) {
  int64_t n;
  in.read(reinterpret_cast<char *>(&n), sizeof(n));
  boost::endian::little_to_native_inplace(n);
  return n;
}

static std::string read_str(std::istream &in) {
  auto size = read_int64(in);
  std::string s(size, '\0');
  in.read(s.data(), size);
  return s;
}

static bool read_bool(std::istream &in) { return in.get() != 0; }

namespace checker::history {

History History::parse_dbcop_history(std::istream &is) {
  constexpr int64_t init_session_id = 0;
  constexpr int64_t init_txn_id = 0;

  History history;
  int64_t current_session_id = 1;
  int64_t current_txn_id = 1;
  std::unordered_set<int64_t> keys;

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
      });
    }
  };

  auto parse_txn = [&](Session &session) {
    Transaction txn;

    auto size = read_int64(is);
    for ([[maybe_unused]] auto i : iota(0, size)) {
      parse_event(txn);
    }

    auto success = read_bool(is);
    if (success) {
      txn.id = current_txn_id++;
      session.transactions.emplace_back(std::move(txn));
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

  auto init_events = keys | transform([](auto key) {
                       return Event{
                           .key = key,
                           .value = 0,
                           .type = EventType::WRITE,
                       };
                     });
  history.sessions.emplace_back(Session{
      .id = init_session_id,
      .transactions = std::vector{Transaction{
          .id = init_txn_id,
          .events = std::vector(init_events.begin(), init_events.end()),
      }},
  });

  return history;
}

std::ostream &operator<<(std::ostream &os, const History &history) {
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

}  // namespace checker::history
