#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <fstream>
#include <mutex>
#include <memory>
#include <functional>
#include <thread>
#include "message.hpp"

namespace lwipc {

/**
 * @brief Bag文件录制器
 * 
 * 支持：
 * - 多topic录制
 * - 时间戳同步
 * - 压缩选项
 * - 分块存储
 */

// Bag文件头
struct BagHeader {
    char magic[9] = "LWIPC001";  // 魔数
    uint32_t version = 1;        // 版本号
    uint64_t start_time = 0;     // 开始时间（纳秒）
    uint64_t end_time = 0;       // 结束时间（纳秒）
    uint32_t message_count = 0;  // 消息总数
    uint32_t topic_count = 0;    // Topic数量
    char compression[16] = "";   // 压缩方式（"none", "lz4", "zstd"）
};

// Topic信息
struct BagTopicInfo {
    std::string name;
    std::string type;
    std::string schema;  // IDL/消息定义
    uint32_t message_count = 0;
};

// 录制的消息
struct BagMessage {
    uint64_t timestamp;      // 时间戳（纳秒）
    std::string topic;       // Topic名称
    uint32_t data_size;      // 数据大小
    std::vector<uint8_t> data;  // 序列化数据
};

class BagRecorder {
public:
    struct Config {
        std::string output_path;
        uint64_t max_file_size = 1024 * 1024 * 1024;  // 1GB
        uint64_t max_duration = 0;  // 最大录制时长（秒），0=无限制
        std::string compression = "none";  // none, lz4, zstd
        bool sync_topics = true;  // 是否同步多topic时间戳
    };
    
private:
    Config config_;
    std::ofstream file_;
    mutable std::mutex mutex_;
    BagHeader header_;
    std::vector<BagTopicInfo> topics_;
    uint64_t start_time_ = 0;
    bool recording_ = false;
    
public:
    BagRecorder() = default;
    ~BagRecorder();
    
    // 配置录制器
    bool configure(const Config& config);
    
    // 添加topic
    bool addTopic(const std::string& name, const std::string& type, const std::string& schema = "");
    
    // 开始录制
    bool start();
    
    // 停止录制
    void stop();
    
    // 录制消息
    bool record(const std::string& topic, uint64_t timestamp, const void* data, size_t size);
    
    // 录制消息（从MessageView）
    bool record(const std::string& topic, const MessageView& msg);
    
    // 是否正在录制
    bool isRecording() const { return recording_; }
    
    // 获取已录制消息数
    uint32_t getMessageCount() const;
    
    // 获取录制时长（秒）
    double getDuration() const;
};

/**
 * @brief Bag文件播放器
 */
class BagPlayer {
public:
    using MessageCallback = std::function<void(const std::string& topic, uint64_t timestamp, const uint8_t* data, size_t size)>;
    
    struct Config {
        bool loop = false;           // 循环播放
        double speed = 1.0;          // 播放速度倍数
        uint64_t start_offset = 0;   // 起始偏移（纳秒）
        uint64_t duration = 0;       // 播放时长（纳秒），0=全部
        bool wait_for_subscribers = false;  // 等待订阅者
    };
    
private:
    std::string file_path_;
    std::ifstream file_;
    mutable std::mutex mutex_;
    BagHeader header_;
    std::vector<BagTopicInfo> topics_;
    Config config_;
    MessageCallback callback_;
    bool playing_ = false;
    bool paused_ = false;
    
public:
    BagPlayer() = default;
    ~BagPlayer();
    
    // 打开bag文件
    bool open(const std::string& path);
    
    // 关闭文件
    void close();
    
    // 设置回调
    void setCallback(MessageCallback cb) { callback_ = std::move(cb); }
    
    // 配置播放器
    void configure(const Config& config) { config_ = config; }
    
    // 开始播放（阻塞）
    bool play();
    
    // 停止播放
    void stop();
    
    // 暂停/继续
    void pause();
    void resume();
    
    // 是否正在播放
    bool isPlaying() const { return playing_ && !paused_; }
    
    // 是否暂停
    bool isPaused() const { return paused_; }
    
    // 获取文件信息
    const BagHeader& getHeader() const { return header_; }
    const std::vector<BagTopicInfo>& getTopics() const { return topics_; }
    
    // 获取播放进度（0.0-1.0）
    double getProgress() const;
};

} // namespace lwipc
