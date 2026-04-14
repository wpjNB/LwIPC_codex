#include "lwipc/ring_buffer_spsc.hpp"

#include <cassert>
#include <cstdint>

int main() {
  lwipc::SpscRingBuffer<int, 4> queue;

  assert(queue.empty());
  assert(queue.try_push(1));
  assert(queue.try_push(2));
  assert(queue.try_push(3));
  assert(!queue.try_push(4));  // full, one slot reserved

  auto v1 = queue.try_pop();
  auto v2 = queue.try_pop();
  auto v3 = queue.try_pop();
  auto v4 = queue.try_pop();

  assert(v1.has_value() && *v1 == 1);
  assert(v2.has_value() && *v2 == 2);
  assert(v3.has_value() && *v3 == 3);
  assert(!v4.has_value());
  assert(queue.empty());

  return 0;
}
