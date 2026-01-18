#ifndef CHECKER_UTILS_UFS_H
#define CHECKER_UTILS_UFS_H

#include <vector>

namespace checker::utils {

struct UFS {
  int n;
  std::vector<int> ufs;

  UFS(int n): n(n), ufs(n) {
    for (auto i = 0; i < n; i++) {
      ufs[i] = i;
    }
  }

  int get(int x) {
    return ufs[x] == x ? x : (ufs[x] = get(ufs[x]));
  }

  void merge(int x, int y) {
    int fx = get(x), fy = get(y);
    if (fx != fy) ufs[fx] = fy;
  }
  
}; // struct UFS

} // namespace checker::utils

#endif // CHECKER_UTILS_UFS_H