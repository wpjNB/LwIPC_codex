#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include "message.hpp"

namespace lwipc {

/**
 * @brief 系统监控模块
 * 
 * 功能：
 * - 节点健康监控
 * - 通信延迟统计
 * - 带宽使用监控
 * - 性能指标采集
 */

// 节点状态
enum class NodeStatus {
    UNKNOWN = 0,
    RUNNING,
    STOPPED,
    ERROR,
    TIMEOUT
};

// 节点信息
struct NodeInfo {
    std::string id;
    std::string name;
    std::string host;
    int pid;
    NodeStatus status;
    uint64_t last_heartbeat;
    uint64_t start_time;
    uint64_t cpu_usage_percent;  // CPU使用率（百分比*100）
    uint64_t memory_usage_bytes; // 内存使用（字节）
    
    NodeInfo() 
        : pid(0)
        , status(NodeStatus::UNKNOWN)
        , last_heartbeat(0)
        , start_time(0)
        , cpu_usage_percent(0)
        , memory_usage_bytes(0)
        {}
};

// 通道统计
struct ChannelStats {
    std::string name;
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t messages_dropped;
    double avg_latency_us;      // 平均延迟（微秒）
    double max_latency_us;      // 最大延迟（微秒）
    double min_latency_us;      // 最小延迟（微秒）
    double p99_latency_us;      // P99延迟（微秒）
    double bandwidth_mbps;      // 带宽（Mbps）
    
    ChannelStats()
        : messages_sent(0)
        , messages_received(0)
        , bytes_sent(0)
        , bytes_received(0)
        , messages_dropped(0)
        , avg_latency_us(0)
        , max_latency_us(0)
        , min_latency_us(0)
        , p99_latency_us(0)
        , bandwidth_mbps(0)
        {}
};

// Topic统计
struct TopicStats {
    std::string name;
    std::string type;
    uint64_t message_count;
    uint64_t subscriber_count;
    uint64_t publisher_count;
    double publish_rate_hz;     // 发布频率（Hz）
    double avg_msg_size_bytes;  // 平均消息大小
    
    TopicStats()
        : message_count(0)
        , subscriber_count(0)
        , publisher_count(0)
        , publish_rate_hz(0)
        , avg_msg_size_bytes(0)
        {}
};

// 系统整体统计
struct SystemStats {
    uint64_t total_nodes;
    uint64_t running_nodes;
    uint64_t total_topics;
    uint64_t total_channels;
    uint64_t total_messages_sent;
    uint64_t total_messages_received;
    uint64_t total_bytes_transferred;
    double system_cpu_usage;
    uint64_t system_memory_used;
    uint64_t system_memory_total;
    
    SystemStats()
        : total_nodes(0)
        , running_nodes(0)
        , total_topics(0)
        , total_channels(0)
        , total_messages_sent(0)
        , total_messages_received(0)
        , total_bytes_transferred(0)
        , system_cpu_usage(0)
        , system_memory_used(0)
        , system_memory_total(0)
        {}
};

// 告警级别
enum class AlertLevel {
    INFO = 0,
    WARNING,
    ERROR,
    CRITICAL
};

// 告警信息
struct Alert {
    AlertLevel level;
    std::string source;      // 告警来源（节点/通道名称）
    std::string message;
    uint64_t timestamp;
    std::string details;
};

// 告警回调
using AlertCallback = std::function<void(const Alert&)>;

/**
 * @brief 系统监控器
 */
class SystemMonitor {
public:
    struct Config {
        uint64_t heartbeat_timeout_ms = 5000;     // 心跳超时时间（毫秒）
        uint64_t stats_interval_ms = 1000;        // 统计间隔（毫秒）
        bool enable_alerts = true;                // 启用告警
        double cpu_threshold = 90.0;              // CPU告警阈值（%）
        double memory_threshold = 90.0;           // 内存告警阈值（%）
        double latency_threshold_us = 1000.0;     // 延迟告警阈值（微秒）
    };
    
private:
    mutable std::mutex mutex_;
    Config config_;
    
    // 节点信息
    std::unordered_map<std::string, NodeInfo> nodes_;
    
    // 通道统计
    std::unordered_map<std::string, ChannelStats> channel_stats_;
    
    // Topic统计
    std::unordered_map<std::string, TopicStats> topic_stats_;
    
    // 告警回调
    AlertCallback alert_callback_;
    
    // 运行状态
    std::atomic<bool> running_{false};
    
    // 延迟采样（用于计算P99）
    std::vector<double> latency_samples_;
    static constexpr size_t MAX_LATENCY_SAMPLES = 1000;
    
public:
    SystemMonitor() = default;
    ~SystemMonitor();
    
    // 配置监控器
    void configure(const Config& config);
    
    // 启动监控
    void start();
    
    // 停止监控
    void stop();
    
    // 是否正在运行
    bool isRunning() const { return running_.load(); }
    
    // 注册节点
    void registerNode(const NodeInfo& node);
    
    // 注销节点
    void unregisterNode(const std::string& node_id);
    
    // 更新节点心跳
    void updateHeartbeat(const std::string& node_id);
    
    // 更新节点资源使用
    void updateNodeResources(const std::string& node_id, 
                            double cpu_percent, 
                            uint64_t memory_bytes);
    
    // 记录消息发送
    void recordMessageSent(const std::string& channel, 
                          uint64_t bytes, 
                          double latency_us);
    
    // 记录消息接收
    void recordMessageReceived(const std::string& channel, uint64_t bytes);
    
    // 记录消息丢弃
    void recordMessageDropped(const std::string& channel);
    
    // 更新Topic信息
    void updateTopicInfo(const std::string& topic, 
                        const std::string& type,
                        uint64_t msg_size);
    
    // 设置告警回调
    void setAlertCallback(AlertCallback cb) { alert_callback_ = std::move(cb); }
    
    // 获取节点列表
    std::vector<NodeInfo> getNodes() const;
    
    // 获取节点信息
    NodeInfo getNode(const std::string& node_id) const;
    
    // 获取通道统计
    std::vector<ChannelStats> getChannelStats() const;
    
    // 获取Topic统计
    std::vector<TopicStats> getTopicStats() const;
    
    // 获取系统统计
    SystemStats getSystemStats() const;
    
    // 检查节点健康状态
    std::vector<NodeInfo> checkHealth() const;
    
    // 获取超时节点
    std::vector<NodeInfo> getTimeoutNodes() const;
};

} // namespace lwipc
