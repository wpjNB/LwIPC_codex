#include "lwipc/executor.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

int main() {
  lwipc::Executor executor;
  std::atomic<int> counter{0};

  executor.post([&] { counter.fetch_add(1, std::memory_order_relaxed); });
  executor.post([&] { counter.fetch_add(2, std::memory_order_relaxed); });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  executor.stop();

  assert(counter.load(std::memory_order_relaxed) == 3);
  return 0;
}
