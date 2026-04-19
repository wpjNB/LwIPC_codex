#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace lwipc {

/**
 * @brief 原子读写锁（Atomic RW Lock）
 * 
 * 针对读多写少场景优化的无锁读写锁
 * - 支持多个读者并发访问
 * - 写者独占访问
 * - 使用原子操作实现，避免传统互斥锁的上下文切换开销
 */
class AtomicRWLock {
 public:
  AtomicRWLock() = default;

  // 禁止拷贝和移动
  AtomicRWLock(const AtomicRWLock&) = delete;
  AtomicRWLock& operator=(const AtomicRWLock&) = delete;
  AtomicRWLock(AtomicRWLock&&) = delete;
  AtomicRWLock& operator=(AtomicRWLock&&) = delete;

  /**
   * @brief 尝试获取读锁
   * @return 成功返回true，失败返回false（有写者持有锁）
   */
  bool try_lock_read() {
    std::uint32_t current = state_.load(std::memory_order_relaxed);
    
    // 检查是否有写者（最高位为1表示写锁定）
    if (current & WRITE_LOCK_BIT) {
      return false;
    }
    
    // 尝试增加读者计数
    while (!state_.compare_exchange_weak(current, current + 1,
                                          std::memory_order_acquire,
                                          std::memory_order_relaxed)) {
      if (current & WRITE_LOCK_BIT) {
        return false;
      }
    }
    
    return true;
  }

  /**
   * @brief 释放读锁
   */
  void unlock_read() {
    state_.fetch_sub(1, std::memory_order_release);
  }

  /**
   * @brief 尝试获取写锁
   * @return 成功返回true，失败返回false（有其他读者或写者）
   */
  bool try_lock_write() {
    std::uint32_t expected = 0;
    // 只有在没有任何读者和写者时才能获取写锁
    return state_.compare_exchange_strong(expected, WRITE_LOCK_BIT,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed);
  }

  /**
   * @brief 释放写锁
   */
  void unlock_write() {
    state_.store(0, std::memory_order_release);
  }

  /**
   * @brief 检查是否有写锁
   */
  [[nodiscard]] bool is_write_locked() const {
    return state_.load(std::memory_order_acquire) & WRITE_LOCK_BIT;
  }

  /**
   * @brief 获取当前读者数量
   */
  [[nodiscard]] std::uint32_t reader_count() const {
    return state_.load(std::memory_order_acquire) & READER_MASK;
  }

 private:
  static constexpr std::uint32_t WRITE_LOCK_BIT = 1u << 31;
  static constexpr std::uint32_t READER_MASK = ~(WRITE_LOCK_BIT);

  alignas(64) std::atomic<std::uint32_t> state_{0};
};

/**
 * @brief RAII 读锁守卫
 */
class ReadLockGuard {
 public:
  explicit ReadLockGuard(AtomicRWLock& lock) : lock_(lock) {
    locked_ = lock_.try_lock_read();
  }

  ~ReadLockGuard() {
    if (locked_) {
      lock_.unlock_read();
    }
  }

  // 禁止拷贝和移动
  ReadLockGuard(const ReadLockGuard&) = delete;
  ReadLockGuard& operator=(const ReadLockGuard&) = delete;

  [[nodiscard]] bool owns_lock() const { return locked_; }
  explicit operator bool() const { return locked_; }

 private:
  AtomicRWLock& lock_;
  bool locked_;
};

/**
 * @brief RAII 写锁守卫
 */
class WriteLockGuard {
 public:
  explicit WriteLockGuard(AtomicRWLock& lock) : lock_(lock) {
    locked_ = lock_.try_lock_write();
  }

  ~WriteLockGuard() {
    if (locked_) {
      lock_.unlock_write();
    }
  }

  // 禁止拷贝和移动
  WriteLockGuard(const WriteLockGuard&) = delete;
  WriteLockGuard& operator=(const WriteLockGuard&) = delete;

  [[nodiscard]] bool owns_lock() const { return locked_; }
  explicit operator bool() const { return locked_; }

 private:
  AtomicRWLock& lock_;
  bool locked_;
};

}  // namespace lwipc
