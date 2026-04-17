#include "lwipc/proto_codec.hpp"

#if defined(LWIPC_WITH_PROTOBUF)

#include "lwipc_frame.pb.h"

#include <cstring>
#include <stdexcept>

namespace lwipc {

std::vector<std::byte> encode_proto(const MessageView& msg) {
  lwipc::proto::Frame frame;
  frame.set_topic_id(msg.header.topic_id);
  frame.set_sequence(msg.header.sequence);
  frame.set_timestamp_ns(msg.header.timestamp_ns);
  frame.set_payload(reinterpret_cast<const char*>(msg.payload.data()), msg.payload.size());

  std::string serialized;
  if (!frame.SerializeToString(&serialized)) {
    throw std::runtime_error("protobuf serialize failed");
  }

  std::vector<std::byte> out(serialized.size());
  std::memcpy(out.data(), serialized.data(), serialized.size());
  return out;
}

MessageHeader decode_proto_header(const std::vector<std::byte>& data) {
  lwipc::proto::Frame frame;
  if (!frame.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
    throw std::runtime_error("protobuf parse failed");
  }

  MessageHeader header;
  header.topic_id = frame.topic_id();
  header.sequence = frame.sequence();
  header.timestamp_ns = frame.timestamp_ns();
  header.payload_len = static_cast<std::uint32_t>(frame.payload().size());
  return header;
}

}  // namespace lwipc

#endif
