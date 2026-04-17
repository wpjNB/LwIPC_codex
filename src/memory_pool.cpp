#include "lwipc/memory_pool.hpp"

#include <stdexcept>

namespace lwipc {

MemoryPool::MemoryPool(std::size_t slot_size, std::size_t slot_count)
    : slot_size_(slot_size), slot_count_(slot_count), storage_(slot_size * slot_count) {
  if (slot_size_ == 0 || slot_count_ == 0) {
    throw std::invalid_argument("slot_size and slot_count must be > 0");
  }

  free_list_.reserve(slot_count_);
  for (std::uint32_t i = 0; i < slot_count_; ++i) {
    free_list_.push_back(static_cast<std::uint32_t>(slot_count_ - 1 - i));
  }
}

std::optional<MemoryPool::Buffer> MemoryPool::acquire() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (free_list_.empty()) {
    return std::nullopt;
  }

  const auto slot_id = free_list_.back();
  free_list_.pop_back();

  return Buffer{
      slot_id,
      storage_.data() + static_cast<std::size_t>(slot_id) * slot_size_,
      slot_size_,
  };
}

void MemoryPool::release(std::uint32_t slot_id) {
  if (slot_id >= slot_count_) {
    throw std::out_of_range("slot id out of range");
  }

  std::lock_guard<std::mutex> lock(mutex_);
  free_list_.push_back(slot_id);
}

std::size_t MemoryPool::free_slots() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return free_list_.size();
}

}  // namespace lwipc
