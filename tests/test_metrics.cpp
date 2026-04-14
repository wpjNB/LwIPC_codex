#include "lwipc/metrics.hpp"

#include <cassert>

int main() {
  lwipc::Metrics metrics;
  metrics.record_published();
  metrics.record_published();
  metrics.record_delivered();
  metrics.record_dropped();
  metrics.record_latency(123456);

  auto s = metrics.snapshot();
  assert(s.published == 2);
  assert(s.delivered == 1);
  assert(s.dropped == 1);
  assert(s.last_latency_ns == 123456);
  return 0;
}
