/**
 * @file example_pubsub.cpp
 * @brief 发布订阅模式示例
 * 
 * 演示如何使用 LwIPC 实现基本的发布订阅通信
 */

#include <lwipc/broker.hpp>
#include <lwipc/message.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

using namespace lwipc;

// 定义消息结构
struct SensorData {
    uint64_t timestamp;
    float value;
    int sensor_id;
};

int main() {
    std::atomic<bool> running{true};
    
    // 创建 Broker
    Broker broker;
    
    // 创建发布者
    QoSProfile qos;
    qos.reliability = Reliability::Reliable;
    auto publisher = std::make_unique<Publisher>(broker, "/sensor/data", qos);
    
    // 创建订阅者
    auto subscriber = std::make_unique<Subscriber>(broker, "/sensor/data", qos, 
        [](const MessageView& msg) {
            const auto* data = reinterpret_cast<const SensorData*>(msg.payload.data());
            std::cout << "[Subscriber] Received: timestamp=" << data->timestamp 
                      << ", value=" << data->value 
                      << ", sensor_id=" << data->sensor_id << std::endl;
        });
    
    // 启动发布线程
    std::thread pub_thread([&]() {
        SensorData data{};
        for (int i = 0; i < 10 && running; ++i) {
            data.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            data.value = static_cast<float>(i) * 1.5f;
            data.sensor_id = 42;
            
            MessageHeader header;
            header.topic_id = 1;
            header.sequence = static_cast<uint64_t>(i);
            header.timestamp_ns = data.timestamp * 1000;  // 转换为纳秒
            header.payload_len = sizeof(SensorData);
            
            MessageView msg;
            msg.header = header;
            msg.payload = PayloadView(reinterpret_cast<const std::byte*>(&data), sizeof(SensorData));
            
            publisher->publish(msg);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    
    // 运行 2 秒
    std::this_thread::sleep_for(std::chrono::seconds(2));
    running = false;
    
    pub_thread.join();
    
    std::cout << "Example completed successfully!" << std::endl;
    return 0;
}
