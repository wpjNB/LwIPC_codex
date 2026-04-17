#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "lwipc/shm_segment.hpp"

namespace lwipc {

class ITransport {
 public:
  virtual ~ITransport() = default;
  virtual bool open() = 0;
  virtual std::size_t send(std::span<const std::byte> payload) = 0;
  virtual std::vector<std::byte> receive() = 0;
};

class ShmTransport final : public ITransport {
 public:
  ShmTransport(std::string shm_name, std::size_t capacity);

  bool open() override;
  std::size_t send(std::span<const std::byte> payload) override;
  std::vector<std::byte> receive() override;

 private:
  std::string shm_name_;
  std::size_t capacity_;
  std::size_t last_size_{0};
  ShmSegment segment_;
};

class UdpTransport final : public ITransport {
 public:
  UdpTransport(std::string bind_ip, std::uint16_t bind_port,
               std::string remote_ip, std::uint16_t remote_port);
  ~UdpTransport() override;

  bool open() override;
  std::size_t send(std::span<const std::byte> payload) override;
  std::vector<std::byte> receive() override;

 private:
  std::string bind_ip_;
  std::uint16_t bind_port_;
  std::string remote_ip_;
  std::uint16_t remote_port_;
  int fd_{-1};
};

}  // namespace lwipc
