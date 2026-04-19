#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "lwipc/ring_buffer_mpmc.hpp"

using namespace lwipc;
using namespace std::chrono_literals;

// 测试基本功能
void test_basic_functionality() {
  std::cout << "=== Test Basic Functionality ===" << std::endl;
  
  MpmcRingBuffer<int> queue(8);
  
  // 测试空队列
  assert(queue.empty());
  assert(queue.size() == 0);
  assert(queue.try_pop() == std::nullopt);
  
  // 测试 push/pop
  assert(queue.try_push(42));
  assert(!queue.empty());
  assert(queue.size() == 1);
  
  auto val = queue.try_pop();
  assert(val.has_value());
  assert(val.value() == 42);
  assert(queue.empty());
  
  // 测试多个元素
  for (int i = 0; i < 5; ++i) {
    assert(queue.try_push(i * 10));
  }
  assert(queue.size() == 5);
  
  for (int i = 0; i < 5; ++i) {
    auto v = queue.try_pop();
    assert(v.has_value());
    assert(v.value() == i * 10);
  }
  
  std::cout << "✓ Basic functionality passed" << std::endl;
}

// 测试单生产者单消费者
void test_spsc_concurrent() {
  std::cout << "=== Test SPSC Concurrent ===" << std::endl;
  
  MpmcRingBuffer<int> queue(1024);
  constexpr int num_items = 10000;
  std::atomic<int> sum{0};
  std::atomic<bool> producer_done{false};
  
  // 生产者线程
  std::thread producer([&]() {
    for (int i = 1; i <= num_items; ++i) {
      while (!queue.try_push(i)) {
        std::this_thread::yield();
      }
    }
    producer_done.store(true, std::memory_order_release);
  });
  
  // 消费者线程
  std::thread consumer([&]() {
    int local_sum = 0;
    int count = 0;
    while (count < num_items) {
      auto val = queue.try_pop();
      if (val.has_value()) {
        local_sum += val.value();
        ++count;
      } else {
        std::this_thread::yield();
      }
    }
    sum.store(local_sum, std::memory_order_release);
  });
  
  producer.join();
  consumer.join();
  
  // 验证总和：1+2+...+10000 = 50005000
  int expected_sum = num_items * (num_items + 1) / 2;
  assert(sum.load() == expected_sum);
  
  std::cout << "✓ SPSC concurrent test passed (sum=" << sum.load() << ")" << std::endl;
}

// 测试多生产者单消费者
void test_mpsc_concurrent() {
  std::cout << "=== Test MPSC Concurrent ===" << std::endl;
  
  MpmcRingBuffer<int> queue(1024);
  constexpr int num_producers = 4;
  constexpr int items_per_producer = 2500;
  std::atomic<int> sum{0};
  std::atomic<int> producers_done{0};
  
  // 多个生产者线程
  std::vector<std::thread> producers;
  for (int p = 0; p < num_producers; ++p) {
    producers.emplace_back([&, p]() {
      int base = p * items_per_producer;
      for (int i = 0; i < items_per_producer; ++i) {
        int value = base + i + 1;
        while (!queue.try_push(value)) {
          std::this_thread::yield();
        }
      }
      producers_done.fetch_add(1, std::memory_order_release);
    });
  }
  
  // 单个消费者线程
  std::thread consumer([&]() {
    int total_items = num_producers * items_per_producer;
    int local_sum = 0;
    int count = 0;
    while (count < total_items) {
      auto val = queue.try_pop();
      if (val.has_value()) {
        local_sum += val.value();
        ++count;
      } else {
        std::this_thread::yield();
      }
    }
    sum.store(local_sum, std::memory_order_release);
  });
  
  for (auto& t : producers) {
    t.join();
  }
  consumer.join();
  
  // 验证总和
  int expected_sum = 0;
  for (int p = 0; p < num_producers; ++p) {
    int base = p * items_per_producer;
    for (int i = 0; i < items_per_producer; ++i) {
      expected_sum += base + i + 1;
    }
  }
  assert(sum.load() == expected_sum);
  
  std::cout << "✓ MPSC concurrent test passed (sum=" << sum.load() << ")" << std::endl;
}

// 测试单生产者多消费者
void test_spmc_concurrent() {
  std::cout << "=== Test SPMC Concurrent ===" << std::endl;
  
  MpmcRingBuffer<int> queue(1024);
  constexpr int num_consumers = 4;
  constexpr int num_items = 10000;
  std::vector<std::atomic<int>> consumer_sums(num_consumers);
  std::atomic<bool> producer_done{false};
  
  // 单个生产者线程
  std::thread producer([&]() {
    for (int i = 1; i <= num_items; ++i) {
      while (!queue.try_push(i)) {
        std::this_thread::yield();
      }
    }
    producer_done.store(true, std::memory_order_release);
  });
  
  // 多个消费者线程
  std::vector<std::thread> consumers;
  for (int c = 0; c < num_consumers; ++c) {
    consumers.emplace_back([&, c]() {
      int local_sum = 0;
      int local_count = 0;
      while (true) {
        auto val = queue.try_pop();
        if (val.has_value()) {
          local_sum += val.value();
          ++local_count;
        } else {
          if (producer_done.load(std::memory_order_acquire) && queue.empty()) {
            break;
          }
          std::this_thread::yield();
        }
      }
      consumer_sums[c].store(local_sum, std::memory_order_release);
    });
  }
  
  producer.join();
  for (auto& t : consumers) {
    t.join();
  }
  
  // 验证所有消费者的总和
  int total_sum = 0;
  for (int c = 0; c < num_consumers; ++c) {
    total_sum += consumer_sums[c].load();
  }
  int expected_sum = num_items * (num_items + 1) / 2;
  assert(total_sum == expected_sum);
  
  std::cout << "✓ SPMC concurrent test passed (total_sum=" << total_sum << ")" << std::endl;
}

// 测试多生产者多消费者
void test_mpmc_concurrent() {
  std::cout << "=== Test MPMC Concurrent ===" << std::endl;
  
  MpmcRingBuffer<int> queue(1024);
  constexpr int num_producers = 4;
  constexpr int num_consumers = 4;
  constexpr int items_per_producer = 2500;
  std::vector<std::atomic<int>> consumer_sums(num_consumers);
  std::atomic<int> producers_done{0};
  
  // 多个生产者线程
  std::vector<std::thread> producers;
  for (int p = 0; p < num_producers; ++p) {
    producers.emplace_back([&, p]() {
      int base = p * items_per_producer;
      for (int i = 0; i < items_per_producer; ++i) {
        int value = base + i + 1;
        while (!queue.try_push(value)) {
          std::this_thread::yield();
        }
      }
      producers_done.fetch_add(1, std::memory_order_release);
    });
  }
  
  // 多个消费者线程
  std::vector<std::thread> consumers;
  for (int c = 0; c < num_consumers; ++c) {
    consumers.emplace_back([&, c]() {
      int local_sum = 0;
      int total_items = num_producers * items_per_producer;
      int local_count = 0;
      while (local_count < total_items / num_consumers + 100) {
        auto val = queue.try_pop();
        if (val.has_value()) {
          local_sum += val.value();
          ++local_count;
        } else {
          if (producers_done.load(std::memory_order_acquire) >= num_producers && 
              queue.empty()) {
            break;
          }
          std::this_thread::yield();
        }
      }
      consumer_sums[c].store(local_sum, std::memory_order_release);
    });
  }
  
  for (auto& t : producers) {
    t.join();
  }
  
  // 等待消费者完成（带超时）
  auto start = std::chrono::steady_clock::now();
  for (auto& t : consumers) {
    auto now = std::chrono::steady_clock::now();
    if (now - start > 10s) {
      std::cerr << "Consumer timeout!" << std::endl;
      break;
    }
    if (t.joinable()) {
      t.join();
    }
  }
  
  // 验证所有消费者的总和
  int total_sum = 0;
  for (int c = 0; c < num_consumers; ++c) {
    total_sum += consumer_sums[c].load();
  }
  int expected_sum = 0;
  for (int p = 0; p < num_producers; ++p) {
    int base = p * items_per_producer;
    for (int i = 0; i < items_per_producer; ++i) {
      expected_sum += base + i + 1;
    }
  }
  assert(total_sum == expected_sum);
  
  std::cout << "✓ MPMC concurrent test passed (total_sum=" << total_sum << ")" << std::endl;
}

// 性能基准测试
void benchmark_throughput() {
  std::cout << "=== Benchmark Throughput ===" << std::endl;
  
  MpmcRingBuffer<std::uint64_t> queue(4096);
  constexpr int num_items = 100000;
  
  auto start = std::chrono::high_resolution_clock::now();
  
  // 生产者
  std::thread producer([&]() {
    for (int i = 0; i < num_items; ++i) {
      while (!queue.try_push(static_cast<std::uint64_t>(i))) {
        std::this_thread::yield();
      }
    }
  });
  
  // 消费者
  std::thread consumer([&]() {
    int count = 0;
    while (count < num_items) {
      auto val = queue.try_pop();
      if (val.has_value()) {
        ++count;
      } else {
        std::this_thread::yield();
      }
    }
  });
  
  producer.join();
  consumer.join();
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  
  double throughput = (num_items * 1000000.0) / duration;  // messages per second
  std::cout << "Throughput: " << static_cast<int>(throughput) << " msg/s" << std::endl;
  std::cout << "Latency: " << (duration * 1000.0 / num_items) << " ns/msg" << std::endl;
  std::cout << "✓ Benchmark completed" << std::endl;
}

int main() {
  std::cout << "MPMC Ring Buffer Tests" << std::endl;
  std::cout << "======================" << std::endl << std::endl;
  
  try {
    test_basic_functionality();
    test_spsc_concurrent();
    test_mpsc_concurrent();
    test_spmc_concurrent();
    test_mpmc_concurrent();
    benchmark_throughput();
    
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
