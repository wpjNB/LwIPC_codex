#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace lwipc {

class MemoryPool {
 public:
  struct Buffer {
    std::uint32_t slot_id{};
    std::byte* data{};
    std::size_t capacity{};
  };

  MemoryPool(std::size_t slot_size, std::size_t slot_count);

  std::optional<Buffer> acquire();
  void release(std::uint32_t slot_id);

  [[nodiscard]] std::size_t slot_size() const { return slot_size_; }
  [[nodiscard]] std::size_t slot_count() const { return slot_count_; }
  [[nodiscard]] std::size_t free_slots() const;

 private:
  std::size_t slot_size_;
  std::size_t slot_count_;
  std::vector<std::byte> storage_;
  std::vector<std::uint32_t> free_list_;
  mutable std::mutex mutex_;
};

}  // namespace lwipc
