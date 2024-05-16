#ifndef MINISAT_MONITOR_H
#define MINISAT_MONITOR_H

#include <cstdint>
#include <unordered_map>

namespace Minisat {
class Monitor {
public:
  int find_cycle_times;
  int propagated_lit_times;
  int64_t add_edge_times;
  int64_t extend_times;
  int64_t cycle_edge_count_sum;
  std::unordered_map<int, int64_t> cycle_width_count;
  int skipped_bridge_count;
  int64_t construct_uep_count;
  int64_t uep_b_size_sum, uep_f_size_sum;
  int64_t propagated_lit_add_times;  

  double var_divide_known_edge_ratio_sum;

  static Monitor *monitor;

  Monitor();
  static Monitor *get_monitor();
  void show_statistics();
};

// TODO: implement more monitor options


} // namespace Minisat::Monitor

#endif // MINISAT_MONITOR_H