#include "lwipc/shm_segment.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>

int main() {
  lwipc::ShmSegment seg("lwipc_test_seg", 4096);
  auto* mem = static_cast<std::uint8_t*>(seg.data());
  assert(mem != nullptr);

  const char* text = "lwipc";
  std::memcpy(mem, text, 6);
  assert(std::memcmp(mem, text, 6) == 0);
  return 0;
}
