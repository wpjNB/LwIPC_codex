#include "lwipc/core.hpp"

#include <array>
#include <atomic>
#include <cassert>

int main() {
  lwipc::Node node("planner");
  lwipc::QoSProfile qos;
  auto channel = node.create_channel("/planning/trajectory", qos);

  std::atomic<int> count{0};
  auto sub = node.subscribe("/planning/trajectory", qos, [&](const lwipc::MessageView& msg) {
    assert(msg.payload.size() == 2);
    count.fetch_add(1, std::memory_order_relaxed);
  });

  std::array<std::byte, 2> payload{std::byte{0xaa}, std::byte{0xbb}};
  lwipc::MessageHeader header;
  header.payload_len = 2;
  channel->publish(lwipc::MessageView{header, payload});

  assert(count.load(std::memory_order_relaxed) == 1);
  assert(channel->topic() == "/planning/trajectory");
  return 0;
}
