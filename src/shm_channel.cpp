#include "lwipc/shm_channel.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace lwipc {

std::unique_ptr<ShmChannel> ShmChannel::create(const ShmChannelConfig& config) {
  auto channel = std::unique_ptr<ShmChannel>(new ShmChannel());
  channel->config_ = config;
  channel->is_creator_ = true;
  if (!channel->initializeCreator()) {
    return nullptr;
  }
  return channel;
}

std::unique_ptr<ShmChannel> ShmChannel::open(const std::string& name) {
  auto channel = std::unique_ptr<ShmChannel>(new ShmChannel());
  channel->config_.name = name;
  channel->is_creator_ = false;
  if (!channel->initializeConsumer()) {
    return nullptr;
  }
  return channel;
}

bool ShmChannel::initializeCreator() {
  // 创建共享内存段（先创建，后初始化控制块）
  std::string shm_name = "/lwipc_" + config_.name;
  
  // 计算槽位大小：控制块 + 消息头 + 最大负载
  std::size_t slot_size = sizeof(ShmMessageSlot) + config_.payload_max;
  std::size_t buffer_size = config_.buffer_size;
  std::size_t total_size = sizeof(ShmChannelControlBlock) + buffer_size * slot_size;
  
  if (!shm_.create(shm_name, total_size)) {
    std::cerr << "Failed to create shared memory: " << shm_name << std::endl;
    return false;
  }
  
  // 获取控制块指针
  control_ = reinterpret_cast<ShmChannelControlBlock*>(shm_.data());
  
  // 初始化控制块成员
  control_->version.store(1, std::memory_order_relaxed);
  control_->initialized.store(false, std::memory_order_relaxed);
  std::memset(control_->channel_name, 0, sizeof(control_->channel_name));
  std::strncpy(control_->channel_name, config_.name.c_str(), sizeof(control_->channel_name) - 1);
  control_->buffer_size = buffer_size;
  control_->payload_max = config_.payload_max;
  control_->slot_size = slot_size;
  control_->total_size = total_size;
  
  // 重置所有计数器
  control_->head.store(0, std::memory_order_relaxed);
  control_->tail.store(0, std::memory_order_relaxed);
  control_->messages_sent.store(0, std::memory_order_relaxed);
  control_->messages_received.store(0, std::memory_order_relaxed);
  control_->messages_dropped.store(0, std::memory_order_relaxed);
  
  // 初始化所有槽位为空
  for (std::size_t i = 0; i < buffer_size; ++i) {
    auto slot = control_->get_slot(i);
    slot->sequence.store(0, std::memory_order_relaxed);
    slot->state.store(ShmMessageSlot::STATE_EMPTY, std::memory_order_relaxed);
  }
  
  // 标记为已初始化（最后一步，确保其他字段先可见）
  control_->initialized.store(true, std::memory_order_release);
  
  return true;
}

bool ShmChannel::initializeConsumer() {
  std::string shm_name = "/lwipc_" + config_.name;
  
  // 打开共享内存段
  if (!shm_.open(shm_name)) {
    std::cerr << "Failed to open shared memory: " << shm_name << std::endl;
    return false;
  }
  
  // 获取控制块指针
  control_ = reinterpret_cast<ShmChannelControlBlock*>(shm_.data());
  
  // 验证通道名称
  if (control_->channel_name != config_.name) {
    std::cerr << "Channel name mismatch" << std::endl;
    return false;
  }
  
  // 更新配置
  config_.buffer_size = control_->buffer_size;
  config_.payload_max = control_->payload_max;
  
  return true;
}

ShmChannel::~ShmChannel() {
  if (is_creator_) {
    destroy();
  }
}

void ShmChannel::destroy() {
  if (control_) {
    // 标记为未初始化
    control_->initialized.store(false, std::memory_order_release);
  }
  shm_.close();
}

bool ShmChannel::publish(const void* data, std::size_t size, std::uint64_t& sequence) {
  if (!control_ || !control_->initialized.load(std::memory_order_acquire)) {
    return false;
  }
  
  if (size > config_.payload_max) {
    std::cerr << "Payload too large: " << size << " > " << config_.payload_max << std::endl;
    return false;
  }
  
  // 获取当前写位置
  std::size_t head = control_->head.load(std::memory_order_relaxed);
  std::size_t tail = control_->tail.load(std::memory_order_acquire);
  
  // 检查缓冲区是否已满
  std::size_t next_head = (head + 1) % control_->buffer_size;
  if (next_head == tail) {
    // 缓冲区满，丢弃消息
    control_->messages_dropped.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  
  // 获取目标槽位
  auto slot = control_->get_slot(head);
  
  // 尝试获取槽位锁（CAS 从 EMPTY 到 WRITING）
  std::uint32_t expected = ShmMessageSlot::STATE_EMPTY;
  if (!slot->state.compare_exchange_strong(expected, 
                                            ShmMessageSlot::STATE_WRITING,
                                            std::memory_order_acq_rel)) {
    // 槽位被占用，丢弃消息
    control_->messages_dropped.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  
  // 写入数据（零拷贝：直接写入共享内存）
  ++local_sequence_;
  sequence = local_sequence_;
  
  slot->header.topic_id = 0;
  slot->header.sequence = sequence;
  slot->header.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  slot->header.payload_len = static_cast<std::uint32_t>(size);
  slot->header.crc32 = 0; // TODO: 计算 CRC
  slot->header.flags = 0;
  
  std::memcpy(slot->payload_data, data, size);
  
  // 发布序列号
  slot->sequence.store(sequence, std::memory_order_release);
  
  // 标记槽位为就绪状态
  slot->state.store(ShmMessageSlot::STATE_READY, std::memory_order_release);
  
  // 更新写指针
  control_->head.store(next_head, std::memory_order_release);
  control_->messages_sent.fetch_add(1, std::memory_order_relaxed);
  
  return true;
}

std::size_t ShmChannel::consume(std::function<void(const MessageView&)> callback) {
  if (!control_ || !control_->initialized.load(std::memory_order_acquire)) {
    return 0;
  }
  
  std::size_t count = 0;
  
  while (true) {
    std::size_t tail = control_->tail.load(std::memory_order_relaxed);
    std::size_t head = control_->head.load(std::memory_order_acquire);
    
    // 检查是否有新消息
    if (tail == head) {
      break; // 无新消息
    }
    
    // 获取目标槽位
    auto slot = control_->get_slot(tail);
    
    // 检查槽位状态
    std::uint32_t state = slot->state.load(std::memory_order_acquire);
    if (state != ShmMessageSlot::STATE_READY) {
      break; // 消息未就绪
    }
    
    // 验证序列号
    std::uint64_t seq = slot->sequence.load(std::memory_order_acquire);
    
    // 构造消息视图（零拷贝：直接从共享内存读取）
    MessageView view{
      .header = slot->header,
      .payload = PayloadView{slot->payload_data, slot->header.payload_len}
    };
    
    // 调用回调
    callback(view);
    
    // 标记槽位为读取中
    slot->state.store(ShmMessageSlot::STATE_READING, std::memory_order_release);
    
    // 移动读指针
    std::size_t next_tail = (tail + 1) % control_->buffer_size;
    control_->tail.store(next_tail, std::memory_order_release);
    
    // 清空槽位
    slot->state.store(ShmMessageSlot::STATE_EMPTY, std::memory_order_release);
    control_->messages_received.fetch_add(1, std::memory_order_relaxed);
    
    ++count;
  }
  
  return count;
}

std::uint64_t ShmChannel::messagesSent() const {
  return control_ ? control_->messages_sent.load(std::memory_order_relaxed) : 0;
}

std::uint64_t ShmChannel::messagesReceived() const {
  return control_ ? control_->messages_received.load(std::memory_order_relaxed) : 0;
}

std::uint64_t ShmChannel::messagesDropped() const {
  return control_ ? control_->messages_dropped.load(std::memory_order_relaxed) : 0;
}

bool ShmChannel::isInitialized() const {
  return control_ && control_->initialized.load(std::memory_order_acquire);
}

bool ShmChannel::waitForInitialization(std::chrono::milliseconds timeout) {
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < timeout) {
    if (isInitialized()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

// ShmTransport 实现
std::shared_ptr<ShmChannel> ShmTransport::createPublisher(const ShmChannelConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = channels_.find(config.name);
  if (it != channels_.end()) {
    return it->second;
  }
  
  auto channel = ShmChannel::create(config);
  if (!channel) {
    return nullptr;
  }
  
  auto shared_channel = std::shared_ptr<ShmChannel>(channel.release());
  channels_[config.name] = shared_channel;
  return shared_channel;
}

std::shared_ptr<ShmChannel> ShmTransport::createSubscriber(const std::string& channel_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = channels_.find(channel_name);
  if (it != channels_.end()) {
    return it->second;
  }
  
  ShmChannelConfig config;
  config.name = channel_name;
  
  auto channel = ShmChannel::open(channel_name);
  if (!channel) {
    return nullptr;
  }
  
  auto shared_channel = std::shared_ptr<ShmChannel>(channel.release());
  channels_[channel_name] = shared_channel;
  return shared_channel;
}

std::vector<std::string> ShmTransport::listChannels() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  std::vector<std::string> names;
  names.reserve(channels_.size());
  for (const auto& [name, _] : channels_) {
    names.push_back(name);
  }
  return names;
}

void ShmTransport::destroyChannel(const std::string& name) {
  std::lock_guard<std::mutex> lock(mutex_);
  channels_.erase(name);
}

} // namespace lwipc
