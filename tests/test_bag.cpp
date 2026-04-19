#include <iostream>
#include <cassert>
#include <fstream>
#include <cstring>
#include "lwipc/bag_recorder.hpp"

using namespace lwipc;

void test_bag_recorder() {
    std::cout << "Testing bag recorder... ";
    
    const std::string test_file = "/tmp/test_recorder.bag";
    
    // 创建录制器
    BagRecorder recorder;
    
    // 配置
    BagRecorder::Config config;
    config.output_path = test_file;
    assert(recorder.configure(config));
    
    // 添加topic
    assert(recorder.addTopic("/sensors/imu", "sensor_msgs/Imu"));
    assert(recorder.addTopic("/sensors/lidar", "sensor_msgs/PointCloud2"));
    
    // 开始录制
    assert(recorder.start());
    assert(recorder.isRecording());
    
    // 录制消息
    uint64_t timestamp = 1000000000ULL;  // 1秒（纳秒）
    const char* data1 = "imu_data_1";
    assert(recorder.record("/sensors/imu", timestamp, data1, strlen(data1)));
    
    timestamp = 1001000000ULL;
    const char* data2 = "lidar_data_1";
    assert(recorder.record("/sensors/lidar", timestamp, data2, strlen(data2)));
    
    timestamp = 1002000000ULL;
    const char* data3 = "imu_data_2";
    assert(recorder.record("/sensors/imu", timestamp, data3, strlen(data3)));
    
    assert(recorder.getMessageCount() == 3);
    
    // 停止录制
    recorder.stop();
    assert(!recorder.isRecording());
    
    // 验证文件存在
    std::ifstream file(test_file, std::ios::binary);
    assert(file.good());
    file.close();
    
    std::cout << "PASSED" << std::endl;
}

void test_bag_player() {
    std::cout << "Testing bag player... ";
    
    const std::string test_file = "/tmp/test_player.bag";
    
    // 先录制一些数据
    {
        BagRecorder recorder;
        BagRecorder::Config config;
        config.output_path = test_file;
        recorder.configure(config);
        recorder.addTopic("/test/topic", "std_msgs/String");
        recorder.start();
        
        for (int i = 0; i < 5; ++i) {
            uint64_t ts = 1000000000ULL + i * 1000000ULL;  // 每秒递增
            std::string data = "message_" + std::to_string(i);
            recorder.record("/test/topic", ts, data.c_str(), data.size());
        }
        
        recorder.stop();
    }
    
    // 播放
    BagPlayer player;
    assert(player.open(test_file));
    
    // 验证文件头
    const auto& header = player.getHeader();
    assert(memcmp(header.magic, "LWIPC001", 8) == 0);
    assert(header.message_count == 5);
    assert(header.topic_count == 1);
    
    // 验证topic信息
    const auto& topics = player.getTopics();
    assert(topics.size() == 1);
    assert(topics[0].name == "/test/topic");
    
    // 设置回调并播放
    int message_count = 0;
    player.setCallback([&message_count](const std::string& topic, uint64_t ts, 
                                        const uint8_t* data, size_t size) {
        message_count++;
    });
    
    // 配置快速播放
    BagPlayer::Config play_config;
    play_config.speed = 10.0;  // 10倍速
    player.configure(play_config);
    
    player.play();
    assert(message_count == 5);
    
    player.close();
    
    std::cout << "PASSED" << std::endl;
}

void test_bag_pause_resume() {
    std::cout << "Testing bag pause/resume... ";
    
    const std::string test_file = "/tmp/test_pause.bag";
    
    // 创建测试文件
    {
        BagRecorder recorder;
        BagRecorder::Config config;
        config.output_path = test_file;
        recorder.configure(config);
        recorder.addTopic("/test", "test");
        recorder.start();
        recorder.record("/test", 1000, "data", 4);
        recorder.stop();
    }
    
    BagPlayer player;
    assert(player.open(test_file));
    
    assert(!player.isPaused());
    player.pause();
    assert(player.isPaused());
    player.resume();
    assert(!player.isPaused());
    
    player.close();
    
    std::cout << "PASSED" << std::endl;
}

void test_invalid_file() {
    std::cout << "Testing invalid file handling... ";
    
    BagPlayer player;
    
    // 不存在的文件
    assert(!player.open("/nonexistent/path/file.bag"));
    
    // 无效格式的文件
    const std::string invalid_file = "/tmp/invalid.bag";
    std::ofstream f(invalid_file);
    f << "This is not a valid bag file";
    f.close();
    
    assert(!player.open(invalid_file));
    
    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "=== Bag Recorder/Player Tests ===" << std::endl;
    
    test_bag_recorder();
    test_bag_player();
    test_bag_pause_resume();
    test_invalid_file();
    
    std::cout << "\nAll tests PASSED!" << std::endl;
    return 0;
}
