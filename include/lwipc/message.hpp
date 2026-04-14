#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace lwipc {

enum class Reliability : std::uint8_t {
  BestEffort,
  Reliable,
};

enum class Durability : std::uint8_t {
  Volatile,
  TransientLocal,
};

struct QoSProfile {
  Reliability reliability{Reliability::BestEffort};
  Durability durability{Durability::Volatile};
  std::size_t keep_last{1};
};

struct MessageHeader {
  std::uint32_t topic_id{};
  std::uint64_t sequence{};
  std::uint64_t timestamp_ns{};
  std::uint32_t payload_len{};
  std::uint32_t crc32{};
  std::uint32_t flags{};
  std::uint16_t abi_version{1};
};

using PayloadView = std::span<const std::byte>;

struct MessageView {
  MessageHeader header{};
  PayloadView payload{};
};

}  // namespace lwipc
