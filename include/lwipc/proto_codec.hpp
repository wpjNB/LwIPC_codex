#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "lwipc/message.hpp"

namespace lwipc {

#if defined(LWIPC_WITH_PROTOBUF)
std::vector<std::byte> encode_proto(const MessageView& msg);
MessageHeader decode_proto_header(const std::vector<std::byte>& data);
#endif

}  // namespace lwipc
