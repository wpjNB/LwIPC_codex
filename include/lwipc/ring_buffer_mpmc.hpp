#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace lwipc {

/**
 * @brief MPMC (Multi-Producer Multi-Consumer) 无锁环形队列
 * 
 * 使用序列号(sequencing)实现线程安全的入队/出队操作
 * 支持多个生产者和多个消费者并发访问
 */
template <typename T>
class MpmcRingBuffer {
 public:
  explicit MpmcRingBuffer(std::size_t capacity);
  ~MpmcRingBuffer();

  // 禁止拷贝
  MpmcRingBuffer(const MpmcRingBuffer&) = delete;
  MpmcRingBuffer& operator=(const MpmcRingBuffer&) = delete;

  // 允许移动
  MpmcRingBuffer(MpmcRingBuffer&& other) noexcept;
  MpmcRingBuffer& operator=(MpmcRingBuffer&& other) noexcept;

  /**
   * @brief 尝试推送元素到队列
   * @param value 要推送的值
   * @return 成功返回true，队列满返回false
   */
  bool try_push(const T& value);
  bool try_push(T&& value);

  /**
   * @brief 尝试从队列弹出元素
   * @return 成功返回包含值的optional，队列为空返回nullopt
   */
  std::optional<T> try_pop();

  /**
   * @brief 检查队列是否为空
   * @note 注意：在并发环境下结果可能立即失效
   */
  [[nodiscard]] bool empty() const;

  /**
   * @brief 获取当前队列大小（近似值）
   */
  [[nodiscard]] std::size_t size() const;

  /**
   * @brief 获取队列容量
   */
  [[nodiscard]] std::size_t capacity() const { return capacity_; }

 private:
  struct Cell {
    std::atomic<std::size_t> sequence;
    T data;
    
    Cell() : sequence(0) {}
  };

  static constexpr std::size_t CACHE_LINE_SIZE = 64;

  alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> enqueue_pos_{0};
  alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> dequeue_pos_{0};
  
  std::size_t capacity_;
  std::unique_ptr<Cell[]> buffer_;

  void initialize_sequences();
};

// 模板实现
template <typename T>
MpmcRingBuffer<T>::MpmcRingBuffer(std::size_t capacity)
    : capacity_(capacity), buffer_(std::make_unique<Cell[]>(capacity)) {
  initialize_sequences();
}

template <typename T>
MpmcRingBuffer<T>::~MpmcRingBuffer() = default;

template <typename T>
MpmcRingBuffer<T>::MpmcRingBuffer(MpmcRingBuffer&& other) noexcept
    : enqueue_pos_(other.enqueue_pos_.load(std::memory_order_relaxed)),
      dequeue_pos_(other.dequeue_pos_.load(std::memory_order_relaxed)),
      capacity_(other.capacity_),
      buffer_(std::move(other.buffer_)) {
  other.capacity_ = 0;
}

template <typename T>
MpmcRingBuffer<T>& MpmcRingBuffer<T>::operator=(MpmcRingBuffer&& other) noexcept {
  if (this != &other) {
    enqueue_pos_.store(other.enqueue_pos_.load(std::memory_order_relaxed), 
                       std::memory_order_relaxed);
    dequeue_pos_.store(other.dequeue_pos_.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
    capacity_ = other.capacity_;
    buffer_ = std::move(other.buffer_);
    other.capacity_ = 0;
  }
  return *this;
}

template <typename T>
void MpmcRingBuffer<T>::initialize_sequences() {
  for (std::size_t i = 0; i < capacity_; ++i) {
    buffer_[i].sequence.store(i, std::memory_order_relaxed);
  }
}

template <typename T>
bool MpmcRingBuffer<T>::try_push(const T& value) {
  std::size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
  
  while (true) {
    Cell& cell = buffer_[pos % capacity_];
    std::size_t seq = cell.sequence.load(std::memory_order_acquire);
    intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
    
    if (diff == 0) {
      // 尝试获取该位置的写入权
      if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, 
                                              std::memory_order_relaxed)) {
        cell.data = value;
        cell.sequence.store(pos + 1, std::memory_order_release);
        return true;
      }
    } else if (diff < 0) {
      // 队列已满
      return false;
    } else {
      // 被其他生产者抢先，重新读取位置
      pos = enqueue_pos_.load(std::memory_order_relaxed);
    }
  }
}

template <typename T>
bool MpmcRingBuffer<T>::try_push(T&& value) {
  std::size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
  
  while (true) {
    Cell& cell = buffer_[pos % capacity_];
    std::size_t seq = cell.sequence.load(std::memory_order_acquire);
    intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
    
    if (diff == 0) {
      if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                                              std::memory_order_relaxed)) {
        cell.data = std::move(value);
        cell.sequence.store(pos + 1, std::memory_order_release);
        return true;
      }
    } else if (diff < 0) {
      return false;
    } else {
      pos = enqueue_pos_.load(std::memory_order_relaxed);
    }
  }
}

template <typename T>
std::optional<T> MpmcRingBuffer<T>::try_pop() {
  std::size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
  
  while (true) {
    Cell& cell = buffer_[pos % capacity_];
    std::size_t seq = cell.sequence.load(std::memory_order_acquire);
    intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
    
    if (diff == 0) {
      // 尝试获取该位置的读取权
      if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
                                              std::memory_order_relaxed)) {
        T value = std::move(cell.data);
        cell.sequence.store(pos + capacity_, std::memory_order_release);
        return value;
      }
    } else if (diff < 0) {
      // 队列为空
      return std::nullopt;
    } else {
      // 被其他消费者抢先，重新读取位置
      pos = dequeue_pos_.load(std::memory_order_relaxed);
    }
  }
}

template <typename T>
bool MpmcRingBuffer<T>::empty() const {
  std::size_t dequeue = dequeue_pos_.load(std::memory_order_acquire);
  std::size_t enqueue = enqueue_pos_.load(std::memory_order_acquire);
  return dequeue >= enqueue;
}

template <typename T>
std::size_t MpmcRingBuffer<T>::size() const {
  std::size_t dequeue = dequeue_pos_.load(std::memory_order_acquire);
  std::size_t enqueue = enqueue_pos_.load(std::memory_order_acquire);
  return (enqueue >= dequeue) ? (enqueue - dequeue) : 0;
}

}  // namespace lwipc
