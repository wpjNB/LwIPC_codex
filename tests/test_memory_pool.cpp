#include "lwipc/memory_pool.hpp"

#include <cassert>
#include <cstdint>

int main() {
  lwipc::MemoryPool pool(256, 2);
  assert(pool.free_slots() == 2);

  auto a = pool.acquire();
  auto b = pool.acquire();
  auto c = pool.acquire();

  assert(a.has_value());
  assert(b.has_value());
  assert(!c.has_value());
  assert(pool.free_slots() == 0);

  a->data[0] = std::byte{0x42};
  pool.release(a->slot_id);
  assert(pool.free_slots() == 1);

  auto d = pool.acquire();
  assert(d.has_value());
  assert(d->capacity == 256);
  assert(pool.free_slots() == 0);

  return 0;
}
