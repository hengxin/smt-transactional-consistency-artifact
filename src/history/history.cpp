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
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>

#include "utils/to_container.h"

namespace fs = std::filesystem;

using std::ranges::count_if;
using std::ranges::views::iota;
using std::ranges::views::transform;
using std::unordered_set;

static auto read_int64(std::istream &in) -> int64_t {
  int64_t n;
  in.read(reinterpret_cast<char *>(&n), sizeof(n));
  boost::endian::little_to_native_inplace(n);
  return n;
}

static auto read_int64_big_endian(std::istream &in) -> int64_t {
  int64_t n = read_int64(in);
  boost::endian::native_to_big_inplace(n);
  return n;
};

static auto read_str(std::istream &in) -> std::string {
  auto size = read_int64(in);
  auto s = std::string(size, '\0');
  in.read(s.data(), size);
  return s;
}

static auto read_char(std::istream &in) -> char {
  // TODO: little-big-endian problem
  return (char)in.get();
}

static auto read_bool(std::istream &in) -> bool { return in.get() != 0; }

static constexpr auto hash_cobra_value = [](const auto &t) {
  auto &[t1, t2, t3] = t;
  std::hash<int64_t> h;
  return h(t1) ^ h(t2) ^ h(t3);
};

namespace checker::history {

auto parse_dbcop_history(std::istream &is) -> History {
  std::cerr << "Not Implemented" << std::endl;
  assert(0);




  // constexpr int64_t init_session_id = 0;
  // constexpr int64_t init_txn_id = 0;

  // auto history = History{};
  // int64_t current_session_id = 1;
  // int64_t current_txn_id = 1;
  // auto keys = std::unordered_set<int64_t>{};

  // auto parse_event = [&](Transaction &txn) {
  //   auto is_write = read_bool(is);
  //   auto key = read_int64(is);
  //   auto value = read_int64(is);
  //   auto success = read_bool(is);

  //   if (success) {
  //     keys.insert(key);
  //     txn.events.emplace_back(Event{
  //         .key = key,
  //         .value = value,
  //         .type = is_write ? EventType::WRITE : EventType::READ,
  //         .transaction_id = txn.id,
  //     });
  //   }
  // };

  // auto parse_txn = [&](Session &session) {
  //   auto &txn = session.transactions.emplace_back(Transaction{
  //       .id = current_txn_id++,
  //       .session_id = session.id,
  //   });

  //   auto size = read_int64(is);
  //   for ([[maybe_unused]] auto i : iota(0, size)) {
  //     parse_event(txn);
  //   }

  //   auto success = read_bool(is);
  //   if (!success) {
  //     session.transactions.pop_back();
  //   }
  // };

  // auto parse_session = [&] {
  //   auto &session =
  //       history.sessions.emplace_back(Session{.id = current_session_id++});

  //   auto size = read_int64(is);
  //   for ([[maybe_unused]] auto i : iota(0, size)) {
  //     parse_txn(session);
  //   }
  // };

  // [[maybe_unused]] auto id = read_int64(is);
  // [[maybe_unused]] auto session_num = read_int64(is);
  // [[maybe_unused]] auto key_num = read_int64(is);
  // [[maybe_unused]] auto txn_num = read_int64(is);
  // [[maybe_unused]] auto event_num = read_int64(is);
  // [[maybe_unused]] auto info = read_str(is);
  // [[maybe_unused]] auto start = read_str(is);
  // [[maybe_unused]] auto end = read_str(is);

  // auto size = read_int64(is);
  // for ([[maybe_unused]] auto i : iota(0, size)) {
  //   parse_session();
  // }

  // history.sessions.emplace_back(Session{
  //     .id = init_session_id,
  //     .transactions = std::vector{Transaction{
  //         .id = init_txn_id,
  //         .events = keys  //
  //                   | transform([](auto key) {
  //                       return Event{
  //                           .key = key,
  //                           .value = 0,
  //                           .type = EventType::WRITE,
  //                           .transaction_id = init_txn_id,
  //                       };
  //                     })  //
  //                   | utils::to<std::vector<Event>>,
  //         .session_id = init_session_id,
  //     }},
  // });

  // // log history meta
  // auto count_all = [](auto &&) { return true; };
  // BOOST_LOG_TRIVIAL(info) << "#sessions: " << history.sessions.size();
  // BOOST_LOG_TRIVIAL(info) << "#transactions: "
  //                         << count_if(history.transactions(), count_all);
  // BOOST_LOG_TRIVIAL(info) << "#events: "
  //                         << count_if(history.events(), count_all);

  // return history;
}

auto parse_cobra_history(const std::string &history_dir) -> History {
  std::cerr << "Not Implemented" << std::endl;
  assert(0);

  // fs::path history_dir_path{history_dir};
  // if (!fs::exists(history_dir_path)) {
  //   std::ostringstream os;
  //   os << "Incorrect path '" << history_dir_path << "'";
  //   throw std::runtime_error{os.str()};
  // }
  // fs::directory_entry history_dir_entry{history_dir_path};

  // static constexpr int64_t INIT_WRITE_ID = 0xbebeebeeL;
  // static constexpr int64_t INIT_TXN_ID = 0xbebeebeeL;
  // static constexpr int64_t NULL_TXN_ID = 0xdeadbeefL;  
  // static constexpr int64_t GC_WID_TRUE = 0x23332333L;
  // static constexpr int64_t GC_WID_FALSE = 0x66666666L;
  
  // auto history = History{};
  // auto init_writes = std::map<int64_t, int64_t>{};
  // int64_t session_id = 0;

  // auto sessions = std::map<int64_t, Session *>{};
  // auto transactions = std::map<int64_t, Transaction *>{};
  // auto writes = std::set<std::pair<int64_t, int64_t>>{}; // <key, value>

  // enum class TransactionStatus { ONGOING, COMMIT };
  // auto transactions_status = std::map<Transaction *, TransactionStatus>{};

  // struct CobraValue {
  //   int64_t write_id;
  //   int64_t transaction_id;
  //   int64_t value;

  //   bool operator == (const CobraValue &rhs) const { 
  //     return write_id == rhs.write_id 
  //         && transaction_id == rhs.transaction_id 
  //         && value == rhs.value; 
  //   }
  // };

  // std::unordered_map<CobraValue, int64_t, decltype(hash_cobra_value)> cobra_value_hash;
  // int64_t cur_hash = 1;
  // auto get_cobra_hash = [&](const CobraValue &cv) -> int64_t {
  //   if (cobra_value_hash.contains(cv)) return cobra_value_hash[cv];
  //   return cobra_value_hash[cv] = cur_hash++;
  // };

  // auto add_session = [&](int64_t id = 0) -> Session * {
  //   if (!id) id = ++session_id;
  //   if (sessions.contains(id)) throw std::runtime_error{"Invalid history: fail in add_session()!"};
  //   Session *session = new Session{};
  //   session->id = id;
  //   sessions[id] = session;
  //   return session;
  // };
  // auto add_transaction = [&](Session *session, int64_t id) -> Transaction * {
  //   if (!sessions.contains(session->id) || transactions.contains(id)) {
  //     throw std::runtime_error{"Invalid history: fail in add_transaction()!"};
  //   }

  //   Transaction *txn = new Transaction{};
  //   txn->id = id, txn->session_id = session->id;
  //   transactions[id] = txn;
  //   transactions_status[txn] = TransactionStatus::ONGOING;
  //   return txn;
  // };
  // auto add_event = [&](Transaction *transaction, EventType type, int64_t key, int64_t value) {
  //   if (type == EventType::WRITE) {
  //     if (!transactions.contains(transaction->id) || writes.contains({key, value})) {
  //       throw std::runtime_error{"Invalid history: fail in add_event()!"};
  //     }
  //     writes.insert({key, value});
  //   }
  //   transaction->events.push_back(Event{key, value, type, transaction->id});
  // };

  // auto parse_log = [&](std::istream &in, Session *session) {
  //   Transaction *current = nullptr;
  //   while (1) {
  //     char op = read_char(in);
  //     if (op == 255 || op == EOF) { // the file ends with 255(EOF), but not reach the real end of this file
  //       // It's quite strange, for 255 = EOF is our common sense, however exception occured on 2 machines
  //       break;
  //     }
  //     // std::cout << op << std::endl;
  //     switch (op) {
  //       case 'S': { 
  //         // txn start 
  //         // assert(current == nullptr);
  //         auto id = read_int64_big_endian(in);
  //         if (!transactions.contains(id)) {
  //           current = add_transaction(session, id);
  //         } else {
  //           current = transactions[id];
  //           // previous txn reads this txn
  //           if (transactions_status[current] == TransactionStatus::ONGOING || current->events.size() != 0) {
  //             throw std::runtime_error{"Invalid history: fail in txn start()!"};
  //           }
  //         }
  //         break;
  //       }
  //       case 'C': {
  //         // txn commit
  //         auto id = read_int64_big_endian(in);
  //         assert(current != nullptr && current->id == id);
  //         transactions_status[current] = TransactionStatus::COMMIT;
  //         Session *session = sessions[current->session_id];
  //         session->transactions.push_back(std::move(*current));
  //         delete current;
  //         break;
  //       }
  //       case 'W': {
  //         // write
  //         assert(current != nullptr);
  //         auto write_id = read_int64_big_endian(in);
  //         auto key = read_int64_big_endian(in);
  //         auto value = read_int64_big_endian(in);
  //         add_event(current, EventType::WRITE, key, get_cobra_hash({write_id, current->id, value}));
  //         break;
  //       }
  //       case 'R': {
  //         // read
  //         assert(current != nullptr);
  //         auto write_txn_id = read_int64_big_endian(in);
  //         auto write_id = read_int64_big_endian(in);
  //         auto key = read_int64_big_endian(in);
  //         auto value = read_int64_big_endian(in);
          
  //         if (write_txn_id == INIT_TXN_ID || write_txn_id == NULL_TXN_ID) {
  //           if (write_id == INIT_WRITE_ID || write_id == NULL_TXN_ID) {
  //             write_id = key;
  //             write_txn_id = INIT_TXN_ID;
  //             if (!init_writes.contains(key)) {
  //               init_writes[key] = get_cobra_hash({key, INIT_TXN_ID, value});
  //               // std::cout << "R (" << key << ", " << value << ")" << std::endl;
  //             }
  //             // TODO: init_writes.computeIfAbsent(key, k -> new CobraValue(key, INIT_TXN_ID, value))
  //           } else if ((write_id != GC_WID_FALSE && write_id != GC_WID_TRUE) || write_txn_id != INIT_TXN_ID) {
  //             throw std::runtime_error{"Invalid history: fail in read()!"};
  //           }
  //         }

  //         add_event(current, EventType::READ, key, get_cobra_hash({write_id, write_txn_id, value}));
  //         break;
  //       }
  //       default: 
  //         throw std::runtime_error{"Invalid history: no opt type is matched!"};
  //     }
  //   }
  // };

  // for (const auto &entry : fs::directory_iterator(history_dir_entry)) {
  //   if (entry.path().extension() != ".log") {
  //     BOOST_LOG_TRIVIAL(trace) << "skip file " << entry.path().filename();
  //     continue;
  //   }
  //   std::string log_file_s = entry.path().string();
  //   auto log_file = std::ifstream{log_file_s};
  //   if (!log_file.is_open()) {
  //     std::ostringstream os;
  //     os << "Cannot open file '" << entry << "'";
  //     throw std::runtime_error{os.str()};
  //   }
  //   BOOST_LOG_TRIVIAL(trace) << "parse file " << entry.path().filename();
  //   try {
  //     auto session = add_session();
  //     parse_log(log_file, session);
  //     history.sessions.push_back(std::move(*session)); // TODO: std::move ?
  //     delete session;
  //   } catch (std::runtime_error &e) {
  //     std::cerr << e.what() << std::endl;
  //     assert(0);
  //   }
  // }

  // auto init_session = add_session(INIT_TXN_ID);
  // Transaction *init_txn = add_transaction(init_session, INIT_TXN_ID);
  // for (const auto &[key, value] : init_writes) {
  //   add_event(init_txn, EventType::WRITE, key, value);
  // }
  // transactions_status[init_txn] = TransactionStatus::COMMIT;
  // init_session->transactions.push_back(std::move(*init_txn));
  // history.sessions.push_back(std::move(*init_session));
  // delete init_session;
  // delete init_txn;

  // for (const auto &[txn_ref, status] : transactions_status) {
  //   if (status == TransactionStatus::ONGOING) {
  //     throw std::runtime_error{"Invalid history: uncommited transaction!"};
  //   }
  // }

  // // log history meta
  // auto count_all = [](auto &&) { return true; };
  // BOOST_LOG_TRIVIAL(info) << "#sessions: " << history.sessions.size();
  // BOOST_LOG_TRIVIAL(info) << "#transactions: "
  //                         << count_if(history.transactions(), count_all);
  // BOOST_LOG_TRIVIAL(info) << "#events: "
  //                         << count_if(history.events(), count_all);

  // return history;
}

auto operator<<(std::ostream &os, const History &history) -> std::ostream & {
  auto out = std::osyncstream{os};

  for (const auto &session : history.sessions) {
    out << "\nSession " << session.id << ":\n";

    for (const auto &txn : session.transactions) {
      out << "Txn " << txn.id << ": ";

      for (const auto &event : txn.events) {
        if (event.type == EventType::READ) {
          out << "R(" << event.key
              << ", [";
          for (const auto &v : event.read_values) {
            out << v << ", ";
          }
          out << "]), ";
        } else { // EventType::WRITE, append actually
          out << "A(" << event.key
              << ", " << event.write_value << "), ";
        }
      }

      out << "\n";
    }
  }

  return os;
}

auto operator<<(std::ostream &os, const InstrumentedHistory &ins_history) -> std::ostream & {
  auto out = std::osyncstream{os};

  auto output_list = [&out](const vector<int64_t> &list) {
    out << "[";
    for (const auto &v : list) out << v << ", ";
    out << "]";
  };

  out << "\nParticipants:\n";
  for (const auto &p_txn : ins_history.participant_txns) {
    out << "txn_id: " << p_txn.id << ": ";
    for (const auto &[key, key_op] : p_txn.key_operations) {
      if (key_op.read_values) {
        out << "R(" << key << ", ";
        output_list(*key_op.read_values);
        out << "), ";
      } 
      if (key_op.write_value) {
        out << "A(" << key << ", " << *key_op.write_value << "), ";
      }
    }
    out << "\n";
  }

  out << "\nObservers:\n";
  for (const auto &o_txn : ins_history.observer_txns) {
    out << "txn_id: " << o_txn.id << ": ";
    out << "R(" << o_txn.key << ", ";
    output_list(o_txn.read_values);
    out << ")\n";
  }

  out << "\nSO Orders:\n";
  for (const auto &[from, to] : ins_history.so_orders) {
    out << from << " -> " << to << ", ";
  }
  out << "\n";

  out << "\nLO Orders:\n";
  for (const auto &[from, to] : ins_history.lo_orders) {
    out << from << " -> " << to << ", ";
  }
  out << "\n";

  return os;
};

auto n_rw_same_key_txns_of(History &history) -> int {
  return 0;
  // int ret = 0;
  // for (auto transaction : history.transactions()) {
  //   std::unordered_map<int64_t, bool> r_keys;
  //   for (auto [key, value, type, _] : transaction.events) {
  //     if (type == EventType::READ) {
  //       r_keys[key] = true;
  //     } else { // type == EventType::WRITE
  //       if (r_keys.contains(key)) {
  //         ++ret;
  //         break;
  //       }
  //     }
  //   }
  // }
  // return ret;
}

auto n_txns_of(History &history) -> int {
  int ret = 0;
  for (auto transaction : history.transactions()) ++ret;
  return ret;
}

auto n_written_key_txns_of(History &history) -> std::unordered_map<int64_t, int> {
  // TODO later
  assert(0);
  // std::unordered_map<int64_t, std::unordered_set<int64_t>> written_txns_of_key;
  // for (auto transaction : history.transactions()) {
  //   for (auto [key, value, type, txn_id] : transaction.events) {
  //     if (type != EventType::WRITE) continue;
  //     written_txns_of_key[key].insert(txn_id);
  //   }
  // }
  // std::unordered_map<int64_t, int> n_written_txns_of_key;
  // for (const auto &[key, txns] : written_txns_of_key) {
  //   n_written_txns_of_key[key] = txns.size();
  // }
  // return n_written_txns_of_key;
}

auto parse_elle_list_append_history(std::ifstream &is) -> History {
  // [sess] [txn_id] [type = 'R' or 'A'] [key] [value]
  // if type == 'R', value = n a1 a2 ... an
  // if type == 'A', value = v

  constexpr int64_t init_session_id = 0;
  constexpr int64_t init_txn_id = 0;
  constexpr int64_t INIT_VALUE = 0x7ff7f7f7f7f7f7f7;

  int n_lines = 0;
  is >> n_lines;

  auto txns = std::unordered_map<int64_t, Transaction>{};
  auto sessions = std::unordered_map<int64_t, std::vector<int64_t>>{}; // sess_id -> { txn_ids in SO order }
  auto init_write_keys = std::set<int64_t> {};
  while (n_lines--) {
    int64_t session_id, txn_id, key, write_value = 0;
    auto read_values = std::vector<int64_t>{};
    std::string type;
    is >> session_id >> txn_id >> type >> key;
    if (type == "R") { // read
      int n_list = 0;
      is >> n_list;
      read_values.assign(n_list + 1, 0);
      read_values[0] = INIT_VALUE;
      for (int i = 1; i <= n_list; i++) is >> read_values[i];
      init_write_keys.insert(key);
    } else if (type == "A") { // append
      is >> write_value;
      if (write_value == INIT_VALUE) {
        BOOST_LOG_TRIVIAL(info) << "WARNING: checker internal magic number INIT_VALUE has been directly written in history, it may leads to strange error";
      }
    }
    if (!txns.contains(txn_id)) {
      txns[txn_id] = Transaction{ .id = txn_id, .events = {}, .session_id = session_id, };
      sessions[session_id].emplace_back(txn_id);
    } 
    txns[txn_id].events.emplace_back((Event) {
      .id = 0,
      .key = key, 
      .write_value = write_value,
      .read_values = read_values, 
      .type = (type == "R" ? EventType::READ : EventType::WRITE),
      .transaction_id = txn_id,
    });
  } 

  auto history = History{};
  history.sessions.emplace_back(Session{
    .id = init_session_id,
    .transactions = std::vector{Transaction{
        .id = init_txn_id,
        .events = init_write_keys  //
                  | transform([](auto key) {
                      return Event{
                          .id = 0,
                          .key = key,
                          .write_value = INIT_VALUE,
                          .read_values = {},
                          .type = EventType::WRITE,
                          .transaction_id = init_txn_id,
                      };
                    })  //
                  | utils::to<std::vector<Event>>,
        .session_id = init_session_id,
    }},
  });
  for (const auto &[sess_id, txn_ids] : sessions) {
    auto session = Session{ .id = sess_id, .transactions = {} };
    for (const auto &txn_id : txn_ids) session.transactions.emplace_back(txns.at(txn_id));
    history.sessions.emplace_back(session);
  }

  // calculate event id
  int64_t event_id_cnt = 0;
  for (auto &sess : history.sessions) {
    for (auto &txn : sess.transactions) {
      for (auto &evt : txn.events) {
        evt.id = ++event_id_cnt;
      }
    }
  }

  return history;
}

auto check_single_write(const History &history) -> bool {
  for (const auto &txn : history.transactions()) {
    auto exist_write = unordered_map<int64_t, bool>{}; // key -> exist write
    for (const auto &[event_id, key, wv, rvs, type, txn_id] : txn.events) {
      if (type == EventType::READ) continue;
      if (exist_write.contains(key)) return false;
      exist_write[key] = true;
    }
  }
  return true;
};

auto check_list_prefix(const History &history) -> bool {
  auto lists = unordered_map<int64_t, vector<int64_t>>{};

  auto is_a_prefix_of = [](const vector<int64_t> &a, const vector<int64_t> &b) -> bool { // a is a prefix of b
    if (a.size() > b.size()) return false;
    for (unsigned i = 0; i < a.size(); i++) {
      if (a[i] != b[i]) return false;
    }
    return true;
  };
  auto check_and_update = [&is_a_prefix_of](vector<int64_t> &source_list, const vector<int64_t> &dest_list) -> bool {
    if (source_list.size() >= dest_list.size()) {
      return is_a_prefix_of(dest_list, source_list);
    } 
    // dest_list.size() > source_list.size()
    bool satisfy_list_prefix = is_a_prefix_of(source_list, dest_list);
    if (!satisfy_list_prefix) return false;
    source_list = dest_list; // TODO: optimization chance, incrementally update
    return true;
  };

  for (const auto &txn : history.transactions()) {
    for (const auto &[event_id, key, wv, rvs, type, txn_id] : txn.events) {
      if (type == EventType::WRITE) continue;
      if (!check_and_update(lists[key], rvs)) return false;
    }
  }
  return true;
}

auto instrumented_history_of(const History &history) -> InstrumentedHistory {
  // check INT axiom and construct instrumented history
  auto ins_history = InstrumentedHistory{};

  auto is_equal_to = [](const vector<int64_t> &a, const vector<int64_t> &b) -> bool {
    if (a.size() != b.size()) return false;
    auto n = a.size();
    for (unsigned i = 0; i < n; i++) {
      if (a[i] != b[i]) return false;
    }
    return true;
  };

  int64_t txn_recount = 0;
  auto id_map = unordered_map<int64_t, int64_t>{}; // txn_id -> new_txn_id
  auto max_length_lists = unordered_map<int64_t, vector<int64_t>>{};
  for (const auto &txn : history.transactions()) {
    auto read_value = unordered_map<int64_t, vector<int64_t>>{}; // key -> list
    auto write_value = unordered_map<int64_t, int64_t>{};
    auto keys = unordered_set<int64_t>{};
    for (const auto &[event_id, key, wv, rvs, type, txn_id] : txn.events) {
      // for each key, before the only write, all rvs are same
      //  after the only write, all rvs' are same, and rvs' = rvs # wv
      keys.insert(key);
      if (type == EventType::WRITE) {
        if (read_value.contains(key)) read_value[key].emplace_back(wv);
        write_value[key] = wv; // last value must be wv
      } else { // type == EventType::READ
        if (!read_value.contains(key)) { // first read
          if (write_value.contains(key)) {
            if (write_value[key] != (*rvs.rbegin())) {
              throw std::runtime_error{"This history violates INT axiom"};
            }
          }
          read_value[key] = rvs; // TODO: optimization chance, incrementally update
        } else { // not first read
          if (!is_equal_to(read_value[key], rvs)) {
            throw std::runtime_error{"This history violates INT axiom"};
          }
        }
      }
      
      if (read_value.contains(key) && max_length_lists[key].size() < read_value[key].size()) {
        max_length_lists[key] = read_value[key]; // to construct observer
      }
    }
    auto participant_txn = ParticipantTransaction{};
    participant_txn.id = txn_recount++;

    for (const auto &key : keys) {
      auto key_operation = KeyOperation{ .key = key, };
      if (!read_value.contains(key)) {
        key_operation.write_value = write_value.at(key);
      } else {
        key_operation.read_values = read_value.at(key);
        if (write_value.contains(key)) {
          assert(write_value[key] == *(key_operation.read_values->rbegin()));
          key_operation.read_values->pop_back();
          key_operation.write_value = write_value.at(key);
        } 
      }
      participant_txn.key_operations[key] = key_operation;
    }

    id_map[txn.id] = participant_txn.id;
    ins_history.participant_txns.emplace_back(participant_txn);
  }

  for (const auto &sess : history.sessions) {
    auto prev_txn = (const Transaction *){};
    for (const auto &txn : sess.transactions) {
      if (prev_txn) {
        ins_history.so_orders.emplace(id_map.at(prev_txn->id), id_map.at(txn.id));
      }

      prev_txn = &txn;
    }
  }

  for (const auto &[key, list] : max_length_lists) {
    auto cur_list = vector<int64_t>{};
    int64_t prev_txn_id = -1;
    for (const auto &v : list) {
      cur_list.emplace_back(v);
      ins_history.observer_txns.emplace_back(ObserverTransaction{
        .id = txn_recount++,
        .key = key,
        .read_values = cur_list,
      });
      if (prev_txn_id != -1) {
        ins_history.lo_orders.emplace(prev_txn_id, txn_recount - 1);
      }
      prev_txn_id = txn_recount - 1;
    }
  }

  return ins_history;
}

auto compute_history_meta_info(const History &history) -> HistoryMetaInfo {
  auto history_meta_info = HistoryMetaInfo{};
  history_meta_info.n_sessions = history.sessions.size();
  for (const auto &txn : history.transactions()) {
    ++history_meta_info.n_total_transactions;
    history_meta_info.n_total_events += txn.events.size();
  }

  int64_t node_cnt = -1; // 0-start index
  for (const auto &txn : history.transactions()) {
    for (const auto &[event_id, key, wv, rvs, type, txn_id] : txn.events) {
      assert(txn_id == txn.id);
      if (type == EventType::READ) {
        assert(wv == 0);
        int64_t index = 0;
        for (const auto &v : rvs) {
          ++node_cnt;
          if (!history_meta_info.begin_node.contains(txn.id)) 
            history_meta_info.begin_node[txn.id] = node_cnt;
          history_meta_info.read_node[event_id][index] = node_cnt;
          history_meta_info.event_value[node_cnt] = v;
          ++index;
        }
      } else { // EventType::WRITE, append
        assert(rvs.empty());
        ++node_cnt;
        if (!history_meta_info.begin_node.contains(txn.id)) 
          history_meta_info.begin_node[txn.id] = node_cnt;
        history_meta_info.write_node[event_id] = node_cnt;
        history_meta_info.event_value[node_cnt] = wv;
      }
    }
    history_meta_info.end_node[txn.id] = node_cnt;
  }

  ++node_cnt; // [0, node_cnt)
  history_meta_info.n_nodes = node_cnt;
  return history_meta_info;
};

auto compute_history_meta_info(const InstrumentedHistory &ins_history) -> HistoryMetaInfo {
  // only consider read_length now
  auto history_meta_info = HistoryMetaInfo{};
  auto &read_length = history_meta_info.read_length; // txn_id -> (key -> length)
  for (const auto &[txn_id, key, rvs] : ins_history.observer_txns) {
    assert(!read_length.contains(txn_id) || !read_length.at(txn_id).contains(key));
    read_length[txn_id][key] = rvs.size();
  }
  for (const auto &[txn_id, ops] : ins_history.participant_txns) {
    for (const auto &[key, op] : ops) {
      if (op.read_values) {
        assert(!read_length.contains(txn_id) || !read_length.at(txn_id).contains(key));
        read_length[txn_id][key] = op.read_values->size();
      }
    }
  }
  return history_meta_info;
}


}  // namespace checker::history
