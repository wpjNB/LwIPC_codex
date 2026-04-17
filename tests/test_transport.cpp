#include "lwipc/transport.hpp"

#include <array>
#include <cassert>
#include <cstddef>

int main() {
  {
    lwipc::ShmTransport shm("lwipc_transport_test", 128);
    assert(shm.open());

    std::array<std::byte, 3> payload{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
    const auto sent = shm.send(payload);
    assert(sent == payload.size());

    auto recv = shm.receive();
    assert(recv.size() == payload.size());
    assert(recv[0] == payload[0]);
  }

  {
    lwipc::UdpTransport rx("127.0.0.1", 35001, "127.0.0.1", 35002);
    lwipc::UdpTransport tx("127.0.0.1", 35002, "127.0.0.1", 35001);
    assert(rx.open());
    assert(tx.open());

    std::array<std::byte, 4> payload{std::byte{0x0a}, std::byte{0x0b}, std::byte{0x0c}, std::byte{0x0d}};
    const auto sent = tx.send(payload);
    assert(sent == payload.size());

    auto recv = rx.receive();
    assert(recv.size() == payload.size());
    assert(recv[2] == payload[2]);
  }

  return 0;
}
