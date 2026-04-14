#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace lwipc {

template <typename T, std::size_t Capacity>
class SpscRingBuffer {
  static_assert(Capacity >= 2, "Capacity must be at least 2");

 public:
  bool try_push(const T& value) {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto next = increment(head);
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    buffer_[head] = value;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool try_push(T&& value) {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto next = increment(head);
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    buffer_[head] = std::move(value);
    head_.store(next, std::memory_order_release);
    return true;
  }

  std::optional<T> try_pop() {
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return std::nullopt;
    }

    auto value = std::move(buffer_[tail]);
    tail_.store(increment(tail), std::memory_order_release);
    return value;
  }

  [[nodiscard]] bool empty() const {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
  }

 private:
  static constexpr std::size_t increment(std::size_t idx) { return (idx + 1) % Capacity; }

  alignas(64) std::array<T, Capacity> buffer_{};
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace lwipc
