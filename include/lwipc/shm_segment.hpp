#pragma once

#include <cstddef>
#include <string>

namespace lwipc {

class ShmSegment {
 public:
  ShmSegment(std::string name, std::size_t size);
  ~ShmSegment();

  ShmSegment(const ShmSegment&) = delete;
  ShmSegment& operator=(const ShmSegment&) = delete;

  ShmSegment(ShmSegment&& other) noexcept;
  ShmSegment& operator=(ShmSegment&& other) noexcept;

  [[nodiscard]] void* data() const { return addr_; }
  [[nodiscard]] std::size_t size() const { return size_; }
  [[nodiscard]] const std::string& name() const { return name_; }

 private:
  void close();

  std::string name_;
  std::size_t size_{};
  int fd_{-1};
  void* addr_{nullptr};
};

}  // namespace lwipc
