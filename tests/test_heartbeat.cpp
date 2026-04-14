#include "lwipc/heartbeat.hpp"

#include <cassert>
#include <chrono>

int main() {
  using namespace std::chrono_literals;
  lwipc::HeartbeatMonitor hb;

  const auto t0 = lwipc::HeartbeatMonitor::Clock::now();
  hb.register_peer("planner", t0);
  hb.register_peer("perception", t0);

  hb.beat("planner", t0 + 40ms);

  auto stale = hb.expired(t0 + 120ms, 60ms);
  // planner: 80ms since last beat -> stale
  // perception: 120ms since register -> stale
  assert(stale.size() == 2);

  auto stale2 = hb.expired(t0 + 70ms, 60ms);
  // planner: 30ms, perception: 70ms -> only perception stale
  assert(stale2.size() == 1);
  assert(stale2[0] == "perception");

  return 0;
}
