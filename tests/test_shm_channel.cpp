#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include "lwipc/shm_channel.hpp"

using namespace lwipc;

void test_basic_publish_subscribe() {
  std::cout << "=== Test: Basic Publish/Subscribe ===" << std::endl;
  
  // 创建发布者通道
  ShmChannelConfig config;
  config.name = "test_topic";
  config.buffer_size = 16;
  config.payload_max = 1024;
  
  auto publisher = ShmChannel::create(config);
  if (!publisher) {
    std::cerr << "Failed to create publisher" << std::endl;
    return;
  }
  
  // 创建订阅者通道
  auto subscriber = ShmChannel::open("test_topic");
  if (!subscriber) {
    std::cerr << "Failed to open subscriber" << std::endl;
    return;
  }
  
  // 等待初始化
  if (!subscriber->waitForInitialization(std::chrono::milliseconds(100))) {
    std::cerr << "Subscriber failed to initialize" << std::endl;
    return;
  }
  
  // 发布消息
  const char* test_data = "Hello, Shared Memory!";
  std::uint64_t sequence = 0;
  
  bool publish_result = publisher->publish(test_data, std::strlen(test_data), sequence);
  if (!publish_result) {
    std::cerr << "Failed to publish message" << std::endl;
    return;
  }
  
  std::cout << "Published message with sequence: " << sequence << std::endl;
  
  // 消费消息
  std::size_t consumed = 0;
  subscriber->consume([&consumed, test_data](const MessageView& msg) {
    ++consumed;
    std::cout << "Received message: sequence=" << msg.header.sequence 
              << ", payload_len=" << msg.header.payload_len << std::endl;
    
    // 验证数据
    const char* received = reinterpret_cast<const char*>(msg.payload.data());
    if (std::memcmp(received, test_data, msg.header.payload_len) == 0) {
      std::cout << "Data verification: PASSED" << std::endl;
    } else {
      std::cout << "Data verification: FAILED" << std::endl;
    }
  });
  
  if (consumed == 0) {
    std::cerr << "No messages consumed" << std::endl;
    return;
  }
  
  // 验证统计信息
  std::cout << "Publisher stats - sent: " << publisher->messagesSent() 
            << ", dropped: " << publisher->messagesDropped() << std::endl;
  std::cout << "Subscriber stats - received: " << subscriber->messagesReceived() << std::endl;
  
  std::cout << "Test PASSED" << std::endl << std::endl;
}

void test_multiple_messages() {
  std::cout << "=== Test: Multiple Messages ===" << std::endl;
  
  ShmChannelConfig config;
  config.name = "test_multi";
  config.buffer_size = 8;
  config.payload_max = 256;
  
  auto publisher = ShmChannel::create(config);
  auto subscriber = ShmChannel::open("test_multi");
  
  if (!publisher || !subscriber) {
    std::cerr << "Failed to create channels" << std::endl;
    return;
  }
  
  subscriber->waitForInitialization(std::chrono::milliseconds(100));
  
  // 发布多条消息
  constexpr int num_messages = 5;
  for (int i = 0; i < num_messages; ++i) {
    std::string msg = "Message #" + std::to_string(i);
    std::uint64_t seq = 0;
    publisher->publish(msg.c_str(), msg.size(), seq);
    std::cout << "Published: " << msg << " (seq=" << seq << ")" << std::endl;
  }
  
  // 批量消费
  std::size_t total_consumed = 0;
  subscriber->consume([&total_consumed](const MessageView& msg) {
    ++total_consumed;
    std::string payload(reinterpret_cast<const char*>(msg.payload.data()), 
                        msg.header.payload_len);
    std::cout << "Consumed: " << payload << std::endl;
  });
  
  std::cout << "Total consumed: " << total_consumed << "/" << num_messages << std::endl;
  
  if (total_consumed == num_messages) {
    std::cout << "Test PASSED" << std::endl;
  } else {
    std::cout << "Test FAILED" << std::endl;
  }
  std::cout << std::endl;
}

void test_buffer_overflow() {
  std::cout << "=== Test: Buffer Overflow Handling ===" << std::endl;
  
  ShmChannelConfig config;
  config.name = "test_overflow";
  config.buffer_size = 4;  // 小缓冲区
  config.payload_max = 64;
  
  auto publisher = ShmChannel::create(config);
  auto subscriber = ShmChannel::open("test_overflow");
  
  if (!publisher || !subscriber) {
    std::cerr << "Failed to create channels" << std::endl;
    return;
  }
  
  subscriber->waitForInitialization(std::chrono::milliseconds(100));
  
  // 发布超过缓冲区容量的消息（不消费）
  constexpr int num_messages = 10;
  int success_count = 0;
  
  for (int i = 0; i < num_messages; ++i) {
    std::string msg = "Msg" + std::to_string(i);
    std::uint64_t seq = 0;
    if (publisher->publish(msg.c_str(), msg.size(), seq)) {
      ++success_count;
    }
  }
  
  std::cout << "Published: " << success_count << "/" << num_messages << std::endl;
  std::cout << "Dropped: " << publisher->messagesDropped() << std::endl;
  
  // 验证：应该只成功发布 buffer_size-1 条消息（留一个空位）
  if (success_count <= config.buffer_size && publisher->messagesDropped() > 0) {
    std::cout << "Test PASSED" << std::endl;
  } else {
    std::cout << "Test FAILED" << std::endl;
  }
  std::cout << std::endl;
}

void test_zero_copy_performance() {
  std::cout << "=== Test: Zero-Copy Performance ===" << std::endl;
  
  ShmChannelConfig config;
  config.name = "test_perf";
  config.buffer_size = 1024;
  config.payload_max = 4096;
  
  auto publisher = ShmChannel::create(config);
  auto subscriber = ShmChannel::open("test_perf");
  
  if (!publisher || !subscriber) {
    std::cerr << "Failed to create channels" << std::endl;
    return;
  }
  
  subscriber->waitForInitialization(std::chrono::milliseconds(100));
  
  // 准备测试数据
  std::vector<char> data(1024, 'A');
  
  // 性能测试：发布
  constexpr int num_messages = 10000;
  auto start = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < num_messages; ++i) {
    std::uint64_t seq = 0;
    publisher->publish(data.data(), data.size(), seq);
  }
  
  auto pub_end = std::chrono::high_resolution_clock::now();
  auto pub_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      pub_end - start).count();
  
  // 性能测试：消费
  std::atomic<std::size_t> consumed_count{0};
  
  start = std::chrono::high_resolution_clock::now();
  
  while (consumed_count.load() < num_messages) {
    subscriber->consume([&consumed_count](const MessageView&) {
      consumed_count.fetch_add(1, std::memory_order_relaxed);
    });
    
    // 短暂休眠避免忙等
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  
  auto sub_end = std::chrono::high_resolution_clock::now();
  auto sub_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      sub_end - start).count();
  
  double pub_throughput = (num_messages * 1000000.0) / pub_duration;
  double sub_throughput = (consumed_count.load() * 1000000.0) / sub_duration;
  
  std::cout << "Published " << num_messages << " messages in " 
            << pub_duration / 1000.0 << " ms" << std::endl;
  std::cout << "Publish throughput: " << pub_throughput << " msg/s" << std::endl;
  std::cout << "Consumed " << consumed_count.load() << " messages in " 
            << sub_duration / 1000.0 << " ms" << std::endl;
  std::cout << "Subscribe throughput: " << sub_throughput << " msg/s" << std::endl;
  
  if (consumed_count.load() == num_messages) {
    std::cout << "Test PASSED" << std::endl;
  } else {
    std::cout << "Test FAILED (lost messages)" << std::endl;
  }
  std::cout << std::endl;
}

void test_shm_transport_manager() {
  std::cout << "=== Test: ShmTransport Manager ===" << std::endl;
  
  ShmTransport transport;
  
  // 创建发布者
  ShmChannelConfig config;
  config.name = "managed_topic";
  config.buffer_size = 32;
  config.payload_max = 512;
  
  auto pub_channel = transport.createPublisher(config);
  if (!pub_channel) {
    std::cerr << "Failed to create managed publisher" << std::endl;
    return;
  }
  
  // 创建订阅者
  auto sub_channel = transport.createSubscriber("managed_topic");
  if (!sub_channel) {
    std::cerr << "Failed to create managed subscriber" << std::endl;
    return;
  }
  
  // 列出通道
  auto channels = transport.listChannels();
  std::cout << "Active channels: ";
  for (const auto& name : channels) {
    std::cout << name << " ";
  }
  std::cout << std::endl;
  
  // 测试通信
  std::string test_msg = "Managed channel test";
  std::uint64_t seq = 0;
  pub_channel->publish(test_msg.c_str(), test_msg.size(), seq);
  
  bool received = false;
  sub_channel->consume([&received](const MessageView& msg) {
    received = true;
    std::cout << "Received via managed channel: " 
              << std::string(reinterpret_cast<const char*>(msg.payload.data()),
                            msg.header.payload_len) << std::endl;
  });
  
  if (received && channels.size() == 1) {
    std::cout << "Test PASSED" << std::endl;
  } else {
    std::cout << "Test FAILED" << std::endl;
  }
  std::cout << std::endl;
}

int main() {
  std::cout << "LwIPC Shared Memory Channel Tests" << std::endl;
  std::cout << "==================================" << std::endl << std::endl;
  
  test_basic_publish_subscribe();
  test_multiple_messages();
  test_buffer_overflow();
  test_zero_copy_performance();
  test_shm_transport_manager();
  
  std::cout << "All tests completed!" << std::endl;
  return 0;
}
