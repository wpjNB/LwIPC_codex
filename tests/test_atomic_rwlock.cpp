#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "lwipc/atomic_rw_lock.hpp"

using namespace lwipc;
using namespace std::chrono_literals;

// 测试基本读写锁功能
void test_basic_rwlock() {
  std::cout << "=== Test Basic RWLock ===" << std::endl;
  
  AtomicRWLock lock;
  
  // 初始状态
  assert(!lock.is_write_locked());
  assert(lock.reader_count() == 0);
  
  // 测试读锁
  {
    ReadLockGuard read_guard(lock);
    assert(read_guard.owns_lock());
    assert(lock.reader_count() == 1);
    assert(!lock.is_write_locked());
    
    // 多个读者
    {
      ReadLockGuard read_guard2(lock);
      assert(read_guard2.owns_lock());
      assert(lock.reader_count() == 2);
    }
    
    // 内层读锁释放后，外层读锁仍然持有
    assert(lock.reader_count() == 1);
  }
  
  // 所有读锁释放后，读者计数应归零
  assert(lock.reader_count() == 0);
  
  // 测试写锁
  {
    WriteLockGuard write_guard(lock);
    assert(write_guard.owns_lock());
    assert(lock.is_write_locked());
    assert(lock.reader_count() == 0);
  }
  
  assert(!lock.is_write_locked());
  assert(lock.reader_count() == 0);
  
  std::cout << "✓ Basic RWLock test passed" << std::endl;
}

// 测试多读者并发
void test_concurrent_readers() {
  std::cout << "=== Test Concurrent Readers ===" << std::endl;
  
  AtomicRWLock lock;
  constexpr int num_readers = 8;
  constexpr int iterations = 10000;
  std::atomic<int> success_count{0};
  std::atomic<int> fail_count{0};
  
  std::vector<std::thread> threads;
  for (int i = 0; i < num_readers; ++i) {
    threads.emplace_back([&]() {
      for (int j = 0; j < iterations; ++j) {
        ReadLockGuard guard(lock);
        if (guard.owns_lock()) {
          // 模拟读操作
          volatile int dummy = lock.reader_count();
          (void)dummy;
          success_count.fetch_add(1, std::memory_order_relaxed);
        } else {
          fail_count.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  assert(success_count.load() == num_readers * iterations);
  assert(fail_count.load() == 0);
  
  std::cout << "✓ Concurrent readers test passed (" 
            << success_count.load() << " successful reads)" << std::endl;
}

// 测试写者独占
void test_writer_exclusivity() {
  std::cout << "=== Test Writer Exclusivity ===" << std::endl;
  
  AtomicRWLock lock;
  std::atomic<bool> writer_active{false};
  std::atomic<int> reader_during_write{0};
  std::atomic<int> write_success{0};
  constexpr int iterations = 1000;
  
  // 写者线程
  std::thread writer([&]() {
    for (int i = 0; i < iterations; ++i) {
      WriteLockGuard guard(lock);
      if (guard.owns_lock()) {
        writer_active.store(true, std::memory_order_release);
        // 模拟写操作
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        writer_active.store(false, std::memory_order_release);
        write_success.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });
  
  // 多个读者线程
  std::vector<std::thread> readers;
  for (int i = 0; i < 4; ++i) {
    readers.emplace_back([&]() {
      for (int j = 0; j < iterations * 10; ++j) {
        ReadLockGuard guard(lock);
        if (guard.owns_lock()) {
          if (writer_active.load(std::memory_order_acquire)) {
            reader_during_write.fetch_add(1, std::memory_order_relaxed);
          }
        }
      }
    });
  }
  
  writer.join();
  for (auto& t : readers) {
    t.join();
  }
  
  // 验证：不应该有读者在写者活动时读取数据
  assert(reader_during_write.load() == 0);
  assert(write_success.load() == iterations);
  
  std::cout << "✓ Writer exclusivity test passed (" 
            << write_success.load() << " successful writes)" << std::endl;
}

// 测试读写竞争
void test_read_write_contention() {
  std::cout << "=== Test Read-Write Contention ===" << std::endl;
  
  AtomicRWLock lock;
  std::atomic<int> read_success{0};
  std::atomic<int> write_success{0};
  std::atomic<bool> stop{false};
  constexpr int num_readers = 4;
  
  // 写者线程
  std::thread writer([&]() {
    while (!stop.load(std::memory_order_relaxed)) {
      WriteLockGuard guard(lock);
      if (guard.owns_lock()) {
        write_success.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::microseconds(5));
      } else {
        std::this_thread::yield();
      }
    }
  });
  
  // 多个读者线程
  std::vector<std::thread> readers;
  for (int i = 0; i < num_readers; ++i) {
    readers.emplace_back([&]() {
      while (!stop.load(std::memory_order_relaxed)) {
        ReadLockGuard guard(lock);
        if (guard.owns_lock()) {
          read_success.fetch_add(1, std::memory_order_relaxed);
        } else {
          std::this_thread::yield();
        }
      }
    });
  }
  
  // 运行一段时间
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stop.store(true, std::memory_order_relaxed);
  
  writer.join();
  for (auto& t : readers) {
    t.join();
  }
  
  std::cout << "✓ Read-write contention test passed (" 
            << read_success.load() << " reads, " 
            << write_success.load() << " writes)" << std::endl;
}

// 性能基准测试
void benchmark_rwlock() {
  std::cout << "=== Benchmark RWLock ===" << std::endl;
  
  AtomicRWLock lock;
  constexpr int iterations = 100000;
  
  // 纯读性能
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    ReadLockGuard guard(lock);
    if (guard.owns_lock()) {
      // 空操作
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto read_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  
  // 纯写性能
  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    WriteLockGuard guard(lock);
    if (guard.owns_lock()) {
      // 空操作
    }
  }
  end = std::chrono::high_resolution_clock::now();
  auto write_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  
  std::cout << "Read-only: " << (read_duration / iterations) << " ns/op" << std::endl;
  std::cout << "Write-only: " << (write_duration / iterations) << " ns/op" << std::endl;
  std::cout << "✓ Benchmark completed" << std::endl;
}

int main() {
  std::cout << "Atomic RWLock Tests" << std::endl;
  std::cout << "===================" << std::endl << std::endl;
  
  try {
    test_basic_rwlock();
    test_concurrent_readers();
    test_writer_exclusivity();
    test_read_write_contention();
    benchmark_rwlock();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "All tests PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Test FAILED: " << e.what() << std::endl;
    return 1;
  }
}
