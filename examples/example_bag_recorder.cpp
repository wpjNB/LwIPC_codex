/**
 * @file example_bag_recorder.cpp
 * @brief 数据录制与回放示例
 * 
 * 演示如何使用 LwIPC 的 bag 录制和回放功能
 */

#include <lwipc/bag_recorder.hpp>
#include <lwipc/broker.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace lwipc;

struct SensorMessage {
    uint64_t timestamp;
    float value;
    int sensor_id;
};

int main() {
    const std::string bag_file = "/tmp/test_recording.bag";
    
    // ========== 录制阶段 ==========
    std::cout << "=== Starting Recording ===" << std::endl;
    
    {
        BagRecorder recorder;
        
        // 开始录制到文件
        if (!recorder.startRecording(bag_file)) {
            std::cerr << "Failed to start recording!" << std::endl;
            return 1;
        }
        
        auto& broker = Broker::instance();
        auto publisher = broker.create_publisher("/sensor/data");
        
        // 发布并录制一些消息
        SensorMessage msg{};
        for (int i = 0; i < 10; ++i) {
            msg.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            msg.value = static_cast<float>(i) * 1.5f;
            msg.sensor_id = 42;
            
            MessageHeader header;
            header.topic_hash = std::hash<std::string>{}("/sensor/data");
            header.timestamp = msg.timestamp;
            header.size = sizeof(SensorMessage);
            
            // 发布消息
            publisher->publish(&msg, header);
            
            // 录制消息
            recorder.record("/sensor/data", &msg, header);
            
            std::cout << "[Recorded] Message " << i << ": value=" << msg.value << std::endl;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // 停止录制
        recorder.stopRecording();
        std::cout << "Recording completed. File: " << bag_file << std::endl;
        
        // 获取录制统计信息
        auto stats = recorder.getStats();
        std::cout << "Total messages recorded: " << stats.total_messages << std::endl;
        std::cout << "Total topics: " << stats.topic_count << std::endl;
        std::cout << "Duration: " << stats.duration_ms << " ms" << std::endl;
    }
    
    // ========== 回放阶段 ==========
    std::cout << "\n=== Starting Playback ===" << std::endl;
    
    {
        BagPlayer player;
        
        // 打开 bag 文件
        if (!player.openBag(bag_file)) {
            std::cerr << "Failed to open bag file!" << std::endl;
            return 1;
        }
        
        std::atomic<int> playback_count{0};
        
        // 设置回放回调
        player.setPlaybackCallback([&](const std::string& topic, const MessageView& msg) {
            const auto* data = static_cast<const SensorMessage*>(msg.data());
            std::cout << "[Playback] Topic: " << topic 
                      << ", value=" << data->value
                      << ", timestamp=" << data->timestamp << std::endl;
            playback_count++;
        });
        
        // 以 1x 速度回放（真实时间）
        std::cout << "Playing back at 1x speed..." << std::endl;
        player.play(1.0f);
        
        // 等待回放完成
        while (playback_count < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        player.stop();
        std::cout << "Playback completed." << std::endl;
        
        // 获取回放统计信息
        auto stats = player.getStats();
        std::cout << "Total messages played: " << stats.messages_played << std::endl;
        std::cout << "Play duration: " << stats.play_duration_ms << " ms" << std::endl;
    }
    
    // ========== 快进回放示例 ==========
    std::cout << "\n=== Fast Forward Playback (2x) ===" << std::endl;
    
    {
        BagPlayer player;
        player.openBag(bag_file);
        
        std::atomic<int> ff_count{0};
        
        player.setPlaybackCallback([&](const std::string& topic, const MessageView& msg) {
            const auto* data = static_cast<const SensorMessage*>(msg.data());
            std::cout << "[FastForward] Message: " << data->value << std::endl;
            ff_count++;
        });
        
        // 以 2x 速度回放
        player.play(2.0f);
        
        while (ff_count < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        player.stop();
        std::cout << "Fast forward playback completed." << std::endl;
    }
    
    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}
