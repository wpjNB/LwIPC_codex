#include "lwipc/bag_recorder.hpp"
#include <cstring>
#include <algorithm>

namespace lwipc {

// ==================== BagRecorder 实现 ====================

BagRecorder::~BagRecorder() {
    stop();
}

bool BagRecorder::configure(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (recording_) {
        return false;  // 录制中不能重新配置
    }
    
    config_ = config;
    
    // 验证配置
    if (config_.output_path.empty()) {
        return false;
    }
    
    return true;
}

bool BagRecorder::addTopic(const std::string& name, const std::string& type, const std::string& schema) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查是否已存在
    for (const auto& topic : topics_) {
        if (topic.name == name) {
            return false;
        }
    }
    
    BagTopicInfo info;
    info.name = name;
    info.type = type;
    info.schema = schema;
    info.message_count = 0;
    
    topics_.push_back(std::move(info));
    header_.topic_count = static_cast<uint32_t>(topics_.size());
    
    return true;
}

bool BagRecorder::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (recording_) {
        return false;
    }
    
    if (config_.output_path.empty()) {
        return false;
    }
    
    // 打开文件
    file_.open(config_.output_path, std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        return false;
    }
    
    // 初始化头信息
    start_time_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    header_.start_time = start_time_;
    header_.end_time = 0;
    header_.message_count = 0;
    header_.topic_count = static_cast<uint32_t>(topics_.size());
    
    if (!config_.compression.empty()) {
        strncpy(header_.compression, config_.compression.c_str(), sizeof(header_.compression) - 1);
    }
    
    // 写入文件头（预留空间）
    file_.write(reinterpret_cast<const char*>(&header_), sizeof(header_));
    
    // 写入topic信息
    for (const auto& topic : topics_) {
        uint32_t name_len = static_cast<uint32_t>(topic.name.size());
        uint32_t type_len = static_cast<uint32_t>(topic.type.size());
        uint32_t schema_len = static_cast<uint32_t>(topic.schema.size());
        
        file_.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        file_.write(topic.name.c_str(), name_len);
        file_.write(reinterpret_cast<const char*>(&type_len), sizeof(type_len));
        file_.write(topic.type.c_str(), type_len);
        file_.write(reinterpret_cast<const char*>(&schema_len), sizeof(schema_len));
        file_.write(topic.schema.c_str(), schema_len);
    }
    
    recording_ = true;
    return true;
}

void BagRecorder::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!recording_) {
        return;
    }
    
    // 更新结束时间
    header_.end_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    // 回到文件开头，更新头信息
    file_.seekp(0, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(&header_), sizeof(header_));
    file_.close();
    
    recording_ = false;
}

bool BagRecorder::record(const std::string& topic, uint64_t timestamp, const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!recording_) {
        return false;
    }
    
    // 写入消息：timestamp(8) + topic_len(4) + topic + data_size(4) + data
    uint32_t topic_len = static_cast<uint32_t>(topic.size());
    uint32_t data_len = static_cast<uint32_t>(size);
    
    file_.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
    file_.write(reinterpret_cast<const char*>(&topic_len), sizeof(topic_len));
    file_.write(topic.c_str(), topic_len);
    file_.write(reinterpret_cast<const char*>(&data_len), sizeof(data_len));
    file_.write(static_cast<const char*>(data), data_len);
    
    header_.message_count++;
    
    // 更新对应topic的计数
    for (auto& t : topics_) {
        if (t.name == topic) {
            t.message_count++;
            break;
        }
    }
    
    return true;
}

bool BagRecorder::record(const std::string& topic, const MessageView& msg) {
    if (msg.payload.empty() || msg.header.timestamp_ns == 0) {
        return false;
    }
    
    uint64_t timestamp = msg.header.timestamp_ns;
    return record(topic, timestamp, msg.payload.data(), msg.payload.size());
}

uint32_t BagRecorder::getMessageCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_.message_count;
}

double BagRecorder::getDuration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (header_.end_time <= header_.start_time) {
        return 0.0;
    }
    
    return static_cast<double>(header_.end_time - header_.start_time) / 1e9;
}

// ==================== BagPlayer 实现 ====================

BagPlayer::~BagPlayer() {
    stop();
    close();
}

bool BagPlayer::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    close();
    
    file_path_ = path;
    file_.open(path, std::ios::binary);
    if (!file_.is_open()) {
        return false;
    }
    
    // 读取文件头
    file_.read(reinterpret_cast<char*>(&header_), sizeof(header_));
    if (!file_.good()) {
        file_.close();
        return false;
    }
    
    // 验证魔数
    if (memcmp(header_.magic, "LWIPC001", 8) != 0) {
        file_.close();
        return false;
    }
    
    // 读取topic信息
    topics_.clear();
    for (uint32_t i = 0; i < header_.topic_count; ++i) {
        uint32_t name_len, type_len, schema_len;
        
        file_.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        std::string name(name_len, '\0');
        file_.read(&name[0], name_len);
        
        file_.read(reinterpret_cast<char*>(&type_len), sizeof(type_len));
        std::string type(type_len, '\0');
        file_.read(&type[0], type_len);
        
        file_.read(reinterpret_cast<char*>(&schema_len), sizeof(schema_len));
        std::string schema(schema_len, '\0');
        file_.read(&schema[0], schema_len);
        
        BagTopicInfo info;
        info.name = name;
        info.type = type;
        info.schema = schema;
        info.message_count = 0;
        topics_.push_back(std::move(info));
    }
    
    return true;
}

void BagPlayer::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (file_.is_open()) {
        file_.close();
    }
    
    topics_.clear();
    playing_ = false;
    paused_ = false;
}

bool BagPlayer::play() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!file_.is_open() || !callback_) {
        return false;
    }
    
    playing_ = true;
    paused_ = false;
    
    uint64_t play_start_time = 0;
    uint64_t first_msg_time = 0;
    
    while (playing_ && file_.good()) {
        if (paused_) {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            lock.lock();
            continue;
        }
        
        // 读取消息
        uint64_t timestamp;
        uint32_t topic_len, data_len;
        
        if (!file_.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp)).good()) {
            break;
        }
        
        if (!file_.read(reinterpret_cast<char*>(&topic_len), sizeof(topic_len)).good()) {
            break;
        }
        
        std::string topic(topic_len, '\0');
        if (!file_.read(&topic[0], topic_len).good()) {
            break;
        }
        
        if (!file_.read(reinterpret_cast<char*>(&data_len), sizeof(data_len)).good()) {
            break;
        }
        
        std::vector<uint8_t> data(data_len);
        if (!file_.read(reinterpret_cast<char*>(data.data()), data_len).good()) {
            break;
        }
        
        double speed = config_.speed;
        auto callback = callback_;
        
        // 时间控制
        if (play_start_time == 0) {
            play_start_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            first_msg_time = timestamp;
        }
        
        // 计算延迟以维持原始时间间隔
        uint64_t expected_time = play_start_time + (timestamp - first_msg_time) / speed;
        uint64_t current_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        
        if (expected_time > current_time) {
            auto sleep_ns = expected_time - current_time;
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
            lock.lock();
            
            if (!playing_) {
                break;
            }
            
            if (paused_) {
                continue;
            }
        }
        
        // 调用回调
        lock.unlock();
        callback(topic, timestamp, data.data(), data.size());
        lock.lock();
    }
    
    playing_ = false;
    
    // 循环播放
    if (config_.loop && playing_) {
        file_.seekg(file_.tellg());  // 重置到消息开始位置
        return play();
    }
    
    return true;
}

void BagPlayer::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    playing_ = false;
}

void BagPlayer::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = true;
}

void BagPlayer::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = false;
}

double BagPlayer::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!file_.is_open() || header_.message_count == 0) {
        return 0.0;
    }
    
    // 简单估算：基于文件位置
    auto current_pos = const_cast<std::ifstream&>(file_).tellg();
    const_cast<std::ifstream&>(file_).seekg(0, std::ios::end);
    auto end_pos = const_cast<std::ifstream&>(file_).tellg();
    const_cast<std::ifstream&>(file_).seekg(current_pos);
    
    if (end_pos <= 0) {
        return 0.0;
    }
    
    return static_cast<double>(current_pos) / static_cast<double>(end_pos);
}

} // namespace lwipc
