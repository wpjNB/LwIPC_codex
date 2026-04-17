#include "lwipc/proto_codec.hpp"

#if defined(LWIPC_WITH_PROTOBUF)

#include <array>
#include <cassert>

int main() {
  std::array<std::byte, 3> payload{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}};

  lwipc::MessageHeader header;
  header.topic_id = 7;
  header.sequence = 42;
  header.timestamp_ns = 1000;
  header.payload_len = 3;

  const auto bytes = lwipc::encode_proto(lwipc::MessageView{header, payload});
  auto decoded = lwipc::decode_proto_header(bytes);

  assert(decoded.topic_id == 7);
  assert(decoded.sequence == 42);
  assert(decoded.timestamp_ns == 1000);
  assert(decoded.payload_len == 3);
  return 0;
}

#else
int main() { return 0; }
#endif
