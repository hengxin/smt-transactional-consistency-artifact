#include <bits/stdc++.h>

using namespace std;

const int n_sessions = 2;
const int n_txns_per_session = 50;
const int n_keys = 20; // n_ops = n_keys;

int txn_id = 0;

auto random_divide(int n, int m) -> vector<vector<int>> {
  vector<int> numbers(n);
  iota(numbers.begin(), numbers.end(), 1);

  random_device rd;
  mt19937 g(rd());
  shuffle(numbers.begin(), numbers.end(), g);

  assert(n % m == 0);
  int groupSize = n / m;

  vector<vector<int>> groups(m);

  for (int i = 0; i < n; ++i) {
    groups[i / groupSize].push_back(numbers[i]);
  }

  return groups;
}

auto gen_session(int sess_id, vector<int> &keys) {
  auto in_session = vector<bool>(n_keys + 1, false);
  for (const auto k : keys) in_session[k] = true;
  auto first_txn = true;
  for (int i = 0; i < n_txns_per_session; i++) {
    if (first_txn) {
      for (const auto k : keys) {
        printf("w(%d,%d,%d,%d)\n", k, 1, sess_id, txn_id);
      }
      first_txn = false;
    } else {
      for (int k = 1; k <= n_keys; k++) {
        if (in_session[k]) printf("w(%d,%d,%d,%d)\n", k, 1, sess_id, txn_id);
        else printf("r(%d,%d,%d,%d)\n", k, 1, sess_id, txn_id);
      }
    }
    ++txn_id;
  }
} 

auto main() -> int {
  // ios::sync_with_stdio(false);
  auto &&keys = random_divide(n_keys, n_sessions);
  for (int session_id = 0; session_id < n_sessions; session_id++) {
    gen_session(session_id, keys[session_id]);
  }
  return 0;
}