#include "lwipc/broker.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>

int main() {
  lwipc::Broker broker;
  lwipc::QoSProfile qos;

  std::atomic<int> hit_count{0};

  {
    lwipc::Subscriber sub(broker, "/camera/front", qos, [&](const lwipc::MessageView& msg) {
      hit_count.fetch_add(1, std::memory_order_relaxed);
      assert(msg.header.payload_len == 4);
      assert(msg.payload.size() == 4);
    });

    lwipc::Publisher pub(broker, "/camera/front", qos);
    std::array<std::byte, 4> payload{std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0x44}};
    lwipc::MessageHeader header;
    header.payload_len = 4;

    pub.publish(lwipc::MessageView{header, payload});
    assert(hit_count.load(std::memory_order_relaxed) == 1);
  }

  // subscriber destructed, should not receive more data
  lwipc::Publisher pub2(broker, "/camera/front", qos);
  std::array<std::byte, 4> payload2{std::byte{0xaa}, std::byte{0xbb}, std::byte{0xcc}, std::byte{0xdd}};
  lwipc::MessageHeader header2;
  header2.payload_len = 4;
  pub2.publish(lwipc::MessageView{header2, payload2});

  assert(hit_count.load(std::memory_order_relaxed) == 1);
  return 0;
}
