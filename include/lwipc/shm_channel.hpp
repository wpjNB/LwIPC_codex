#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "lwipc/message.hpp"
#include "lwipc/ring_buffer_mpmc.hpp"
#include "lwipc/shm_segment.hpp"

namespace lwipc {

/// @brief 共享内存通道配置
struct ShmChannelConfig {
  std::string name;              // 通道名称
  std::size_t buffer_size{4096}; // 环形缓冲区大小（消息数）
  std::size_t payload_max{65536}; // 最大负载大小（字节）
  QoSProfile qos;                // QoS策略
};

/// @brief 共享内存中的消息槽位
struct ShmMessageSlot {
  std::atomic<std::uint64_t> sequence{0};
  std::atomic<std::uint32_t> state{0}; // 0: empty, 1: writing, 2: ready, 3: reading
  MessageHeader header;
  std::byte payload_data[]; // 柔性数组

  static constexpr std::uint32_t STATE_EMPTY = 0;
  static constexpr std::uint32_t STATE_WRITING = 1;
  static constexpr std::uint32_t STATE_READY = 2;
  static constexpr std::uint32_t STATE_READING = 3;
};

/// @brief 共享内存通道控制块
struct ShmChannelControlBlock {
  std::atomic<std::uint32_t> version{1};
  std::atomic<bool> initialized{false};
  char channel_name[64];
  std::size_t buffer_size;
  std::size_t payload_max;
  std::size_t slot_size;
  std::size_t total_size;
  
  // 环形缓冲区索引（无锁）
  std::atomic<std::size_t> head{0}; // 写指针
  std::atomic<std::size_t> tail{0}; // 读指针
  
  // 统计信息
  std::atomic<std::uint64_t> messages_sent{0};
  std::atomic<std::uint64_t> messages_received{0};
  std::atomic<std::uint64_t> messages_dropped{0};
  
  // 槽位数据起始偏移
  std::size_t slots_offset() const { return sizeof(ShmChannelControlBlock); }
  
  ShmMessageSlot* get_slot(std::size_t index) {
    auto base = reinterpret_cast<std::byte*>(this) + slots_offset();
    return reinterpret_cast<ShmMessageSlot*>(base + index * slot_size);
  }
};

/// @brief 共享内存传输通道
class ShmChannel {
 public:
  /// @brief 创建或打开共享内存通道（生产者端）
  static std::unique_ptr<ShmChannel> create(const ShmChannelConfig& config);
  
  /// @brief 打开现有共享内存通道（消费者端）
  static std::unique_ptr<ShmChannel> open(const std::string& name);
  
  ~ShmChannel();
  
  /// @brief 发布消息（零拷贝）
  /// @return 成功返回true，缓冲区满返回false
  bool publish(const void* data, std::size_t size, std::uint64_t& sequence);
  
  /// @brief 消费消息（零拷贝回调）
  /// @param callback 回调函数，接收消息视图
  /// @return 消费的消息数量
  std::size_t consume(std::function<void(const MessageView&)> callback);
  
  /// @brief 获取通道名称
  const std::string& name() const { return config_.name; }
  
  /// @brief 获取统计信息
  std::uint64_t messagesSent() const;
  std::uint64_t messagesReceived() const;
  std::uint64_t messagesDropped() const;
  
  /// @brief 检查是否已初始化
  bool isInitialized() const;
  
  /// @brief 等待通道就绪（消费者用）
  bool waitForInitialization(std::chrono::milliseconds timeout);

 private:
  bool initializeCreator();
  bool initializeConsumer();
  void destroy();
  
  ShmChannelConfig config_;
  ShmSegment shm_;
  ShmChannelControlBlock* control_{nullptr};
  bool is_creator_{false};
  std::uint64_t local_sequence_{0};
};

/// @brief 共享内存传输管理器
class ShmTransport {
 public:
  /// @brief 创建发布通道
  std::shared_ptr<ShmChannel> createPublisher(const ShmChannelConfig& config);
  
  /// @brief 创建订阅通道
  std::shared_ptr<ShmChannel> createSubscriber(const std::string& channel_name);
  
  /// @brief 获取所有活跃通道
  std::vector<std::string> listChannels() const;
  
  /// @brief 销毁通道
  void destroyChannel(const std::string& name);

 private:
  std::unordered_map<std::string, std::shared_ptr<ShmChannel>> channels_;
  mutable std::mutex mutex_;
};

} // namespace lwipc
