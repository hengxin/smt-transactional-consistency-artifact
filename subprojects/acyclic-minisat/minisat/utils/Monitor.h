#ifndef MINISAT_MONITOR_H
#define MINISAT_MONITOR_H

#include <cstdint>

namespace Minisat {
class Monitor {
public:
  int find_cycle_times;
  int propagated_lit_times;
  int64_t add_edge_times;
  int64_t extend_times;
  int64_t cycle_edge_count_sum;
  int skipped_bridge_count;
  

  static Monitor *monitor;

  Monitor();
  static Monitor *get_monitor();
  void show_statistics();
};

// TODO: implement more monitor options


} // namespace Minisat::Monitor

#endif // MINISAT_MONITOR_H