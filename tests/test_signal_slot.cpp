#include <iostream>
#include <cassert>
#include <atomic>
#include "lwipc/signal_slot.hpp"

using namespace lwipc;

void test_basic_signal_slot() {
    std::cout << "Testing basic signal/slot... ";
    
    Signal<int> signal;
    int result = 0;
    
    // 连接槽函数
    auto conn = signal.connect([&result](int value) {
        result += value;
    });
    
    assert(conn.isConnected());
    assert(signal.getConnectionCount() == 1);
    
    // 发射信号
    signal.emit(10);
    assert(result == 10);
    
    signal.emit(20);
    assert(result == 30);
    
    // 断开连接
    conn.disconnect();
    assert(!conn.isConnected());
    
    signal.emit(100);  // 应该不会影响result
    assert(result == 30);
    assert(signal.getConnectionCount() == 0);
    
    std::cout << "PASSED" << std::endl;
}

void test_multiple_slots() {
    std::cout << "Testing multiple slots... ";
    
    Signal<double> signal;
    double sum = 0;
    int call_count = 0;
    
    signal.connect([&sum](double v) { sum += v; });
    signal.connect([&call_count](double) { call_count++; });
    signal.connect([&sum, &call_count](double v) { 
        sum *= 2; 
        call_count++; 
    });
    
    assert(signal.getConnectionCount() == 3);
    
    signal.emit(5.0);
    // 执行顺序：sum += 5 (sum=5), call_count++, sum *= 2 (sum=10), call_count++
    assert(sum == 10.0);  // (0 + 5) * 2 = 10
    assert(call_count == 2);
    
    std::cout << "PASSED" << std::endl;
}

void test_disconnect_specific() {
    std::cout << "Testing disconnect specific slot... ";
    
    Signal<std::string> signal;
    std::string log;
    
    auto conn1 = signal.connect([&log](const std::string& s) { log += "1:" + s + ","; });
    auto conn2 = signal.connect([&log](const std::string& s) { log += "2:" + s + ","; });
    auto conn3 = signal.connect([&log](const std::string& s) { log += "3:" + s + ","; });
    
    signal.emit("A");
    assert(log == "1:A,2:A,3:A,");
    
    conn2.disconnect();
    assert(signal.getConnectionCount() == 2);
    
    log.clear();
    signal.emit("B");
    assert(log == "1:B,3:B,");
    
    std::cout << "PASSED" << std::endl;
}

void test_signal_manager() {
    std::cout << "Testing signal manager... ";
    
    SignalManager manager;
    
    // 创建信号
    auto sig1 = manager.getOrCreate<int>("topic1");
    auto sig2 = manager.getOrCreate<double>("topic2");
    
    assert(manager.hasSignal("topic1"));
    assert(manager.hasSignal("topic2"));
    assert(!manager.hasSignal("topic3"));
    
    assert(manager.getSignalCount() == 2);
    
    // 获取已存在的信号
    auto sig1_again = manager.getOrCreate<int>("topic1");
    assert(sig1 == sig1_again);
    
    // 移除信号
    manager.removeSignal("topic1");
    assert(!manager.hasSignal("topic1"));
    assert(manager.getSignalCount() == 1);
    
    std::cout << "PASSED" << std::endl;
}

void test_thread_safety() {
    std::cout << "Testing thread safety... ";
    
    Signal<int> signal;
    std::atomic<int> counter{0};
    
    // 多个线程同时连接
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&signal, &counter]() {
            for (int j = 0; j < 100; ++j) {
                signal.connect([&counter](int) { counter++; });
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    assert(signal.getConnectionCount() == 1000);
    
    // 发射信号
    signal.emit(1);
    assert(counter.load() == 1000);
    
    std::cout << "PASSED" << std::endl;
}

void test_exception_handling() {
    std::cout << "Testing exception handling... ";
    
    Signal<> signal;
    int normal_slot_called = 0;
    
    signal.connect([]() {
        throw std::runtime_error("Test exception");
    });
    
    signal.connect([&normal_slot_called]() {
        normal_slot_called++;
    });
    
    // 异常不应该阻止其他槽被调用
    signal.emit();
    assert(normal_slot_called == 1);
    
    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "=== Signal/Slot Tests ===" << std::endl;
    
    test_basic_signal_slot();
    test_multiple_slots();
    test_disconnect_specific();
    test_signal_manager();
    test_thread_safety();
    test_exception_handling();
    
    std::cout << "\nAll tests PASSED!" << std::endl;
    return 0;
}
