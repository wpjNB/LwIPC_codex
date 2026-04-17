#include "lwipc/transport.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace lwipc {

ShmTransport::ShmTransport(std::string shm_name, std::size_t capacity)
    : shm_name_(std::move(shm_name)), capacity_(capacity), segment_(shm_name_, capacity_) {}

bool ShmTransport::open() { return true; }

std::size_t ShmTransport::send(std::span<const std::byte> payload) {
  if (payload.size() > capacity_) {
    throw std::runtime_error("payload too large for shm transport");
  }

  std::memcpy(segment_.data(), payload.data(), payload.size());
  last_size_ = payload.size();
  return payload.size();
}

std::vector<std::byte> ShmTransport::receive() {
  std::vector<std::byte> out(last_size_);
  std::memcpy(out.data(), segment_.data(), last_size_);
  return out;
}

UdpTransport::UdpTransport(std::string bind_ip, std::uint16_t bind_port,
                           std::string remote_ip, std::uint16_t remote_port)
    : bind_ip_(std::move(bind_ip)),
      bind_port_(bind_port),
      remote_ip_(std::move(remote_ip)),
      remote_port_(remote_port) {}

UdpTransport::~UdpTransport() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

bool UdpTransport::open() {
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0) {
    return false;
  }

  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_port = htons(bind_port_);
  if (inet_pton(AF_INET, bind_ip_.c_str(), &local.sin_addr) <= 0) {
    return false;
  }

  int opt = 1;
  setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if (bind(fd_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
    return false;
  }

  timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = 200000;
  setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  return true;
}

std::size_t UdpTransport::send(std::span<const std::byte> payload) {
  sockaddr_in remote{};
  remote.sin_family = AF_INET;
  remote.sin_port = htons(remote_port_);
  if (inet_pton(AF_INET, remote_ip_.c_str(), &remote.sin_addr) <= 0) {
    return 0;
  }

  const auto sent = sendto(fd_, payload.data(), payload.size(), 0,
                           reinterpret_cast<sockaddr*>(&remote), sizeof(remote));
  return sent > 0 ? static_cast<std::size_t>(sent) : 0;
}

std::vector<std::byte> UdpTransport::receive() {
  std::vector<std::byte> buf(65536);
  const auto n = recv(fd_, buf.data(), buf.size(), 0);
  if (n <= 0) {
    return {};
  }
  buf.resize(static_cast<std::size_t>(n));
  return buf;
}

}  // namespace lwipc
