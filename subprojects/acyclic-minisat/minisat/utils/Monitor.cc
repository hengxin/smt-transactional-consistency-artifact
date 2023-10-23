#include "Monitor.h"

#include <iostream>

namespace Minisat {

Monitor *Monitor::monitor = nullptr;

Monitor::Monitor() {
  find_cycle_times = 0;
  propagated_lit_times = 0;
  add_edge_times = 0;
  extend_times = 0;
  skipped_bridge_count = 0;
  cycle_edge_count_sum = 0;
}

Monitor *Monitor::get_monitor() {
  if (monitor == nullptr) monitor = new Monitor();
  return monitor;
}

void Monitor::show_statistics() {
  std::cerr << "find_cycle_times: " << find_cycle_times << "\n";
  std::cerr << "propagated_lit_times: " << propagated_lit_times << "\n";
  std::cerr << "add_edge_times: " << add_edge_times << "\n";
  std::cerr << "extend_times: " << extend_times << "\n";
  std::cerr << "#skipped bridge = " << skipped_bridge_count << "\n";
  if (find_cycle_times != 0) std::cerr << "avg cycle length = " << 1.0 * cycle_edge_count_sum / find_cycle_times << "\n";
  // TODO
}

} // namespace Minisat::Monitor