#pragma once

#include <atomic>
#include <cstdint>

namespace lwipc {

struct MetricsSnapshot {
  std::uint64_t published{};
  std::uint64_t delivered{};
  std::uint64_t dropped{};
  std::uint64_t last_latency_ns{};
};

class Metrics {
 public:
  void record_published() { published_.fetch_add(1, std::memory_order_relaxed); }
  void record_delivered() { delivered_.fetch_add(1, std::memory_order_relaxed); }
  void record_dropped() { dropped_.fetch_add(1, std::memory_order_relaxed); }
  void record_latency(std::uint64_t ns) { last_latency_ns_.store(ns, std::memory_order_relaxed); }

  [[nodiscard]] MetricsSnapshot snapshot() const {
    return MetricsSnapshot{
        published_.load(std::memory_order_relaxed),
        delivered_.load(std::memory_order_relaxed),
        dropped_.load(std::memory_order_relaxed),
        last_latency_ns_.load(std::memory_order_relaxed),
    };
  }

 private:
  std::atomic<std::uint64_t> published_{0};
  std::atomic<std::uint64_t> delivered_{0};
  std::atomic<std::uint64_t> dropped_{0};
  std::atomic<std::uint64_t> last_latency_ns_{0};
};

}  // namespace lwipc
