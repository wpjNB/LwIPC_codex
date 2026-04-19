#include "lwipc/system_monitor.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace lwipc {

SystemMonitor::~SystemMonitor() {
    stop();
}

void SystemMonitor::configure(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void SystemMonitor::start() {
    running_.store(true);
}

void SystemMonitor::stop() {
    running_.store(false);
}

void SystemMonitor::registerNode(const NodeInfo& node) {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_[node.id] = node;
}

void SystemMonitor::unregisterNode(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_.erase(node_id);
}

void SystemMonitor::updateHeartbeat(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        it->second.last_heartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        it->second.status = NodeStatus::RUNNING;
    }
}

void SystemMonitor::updateNodeResources(const std::string& node_id, 
                                       double cpu_percent, 
                                       uint64_t memory_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        it->second.cpu_usage_percent = static_cast<uint64_t>(cpu_percent * 100);
        it->second.memory_usage_bytes = memory_bytes;
        
        // 检查是否超过阈值
        if (config_.enable_alerts && alert_callback_) {
            if (cpu_percent > config_.cpu_threshold) {
                Alert alert;
                alert.level = AlertLevel::WARNING;
                alert.source = node_id;
                alert.message = "CPU usage exceeded threshold";
                alert.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                alert.details = "CPU: " + std::to_string(cpu_percent) + "%";
                alert_callback_(alert);
            }
            
            // 内存阈值检查（简化：假设总内存为8GB）
            constexpr uint64_t TOTAL_MEMORY = 8ULL * 1024 * 1024 * 1024;
            double memory_percent = (static_cast<double>(memory_bytes) / TOTAL_MEMORY) * 100.0;
            if (memory_percent > config_.memory_threshold) {
                Alert alert;
                alert.level = AlertLevel::WARNING;
                alert.source = node_id;
                alert.message = "Memory usage exceeded threshold";
                alert.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                alert.details = "Memory: " + std::to_string(memory_bytes / 1024 / 1024) + "MB";
                alert_callback_(alert);
            }
        }
    }
}

void SystemMonitor::recordMessageSent(const std::string& channel, 
                                     uint64_t bytes, 
                                     double latency_us) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& stats = channel_stats_[channel];
    stats.name = channel;
    stats.messages_sent++;
    stats.bytes_sent += bytes;
    
    // 更新延迟统计
    if (stats.messages_sent == 1) {
        stats.min_latency_us = latency_us;
        stats.max_latency_us = latency_us;
    } else {
        stats.min_latency_us = std::min(stats.min_latency_us, latency_us);
        stats.max_latency_us = std::max(stats.max_latency_us, latency_us);
    }
    
    // 简单移动平均
    stats.avg_latency_us = (stats.avg_latency_us * (stats.messages_sent - 1) + latency_us) 
                          / stats.messages_sent;
    
    // 采样用于P99计算
    if (latency_samples_.size() >= MAX_LATENCY_SAMPLES) {
        latency_samples_.erase(latency_samples_.begin());
    }
    latency_samples_.push_back(latency_us);
    
    // 检查延迟阈值
    if (config_.enable_alerts && alert_callback_ && latency_us > config_.latency_threshold_us) {
        Alert alert;
        alert.level = AlertLevel::WARNING;
        alert.source = channel;
        alert.message = "Latency exceeded threshold";
        alert.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        alert.details = "Latency: " + std::to_string(latency_us) + "us";
        alert_callback_(alert);
    }
}

void SystemMonitor::recordMessageReceived(const std::string& channel, uint64_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& stats = channel_stats_[channel];
    stats.name = channel;
    stats.messages_received++;
    stats.bytes_received += bytes;
}

void SystemMonitor::recordMessageDropped(const std::string& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& stats = channel_stats_[channel];
    stats.name = channel;
    stats.messages_dropped++;
}

void SystemMonitor::updateTopicInfo(const std::string& topic, 
                                   const std::string& type,
                                   uint64_t msg_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& stats = topic_stats_[topic];
    stats.name = topic;
    stats.type = type;
    stats.message_count++;
    
    // 移动平均消息大小
    stats.avg_msg_size_bytes = (stats.avg_msg_size_bytes * (stats.message_count - 1) + msg_size) 
                              / stats.message_count;
    
    // 简化：假设每秒更新一次，发布率=消息数
    // 实际应用中应该基于时间窗口计算
    stats.publish_rate_hz = static_cast<double>(stats.message_count);
}

std::vector<NodeInfo> SystemMonitor::getNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<NodeInfo> result;
    result.reserve(nodes_.size());
    
    for (const auto& [id, node] : nodes_) {
        result.push_back(node);
    }
    
    return result;
}

NodeInfo SystemMonitor::getNode(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        return it->second;
    }
    
    return NodeInfo();
}

std::vector<ChannelStats> SystemMonitor::getChannelStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ChannelStats> result;
    result.reserve(channel_stats_.size());
    
    for (const auto& [name, stats] : channel_stats_) {
        result.push_back(stats);
    }
    
    return result;
}

std::vector<TopicStats> SystemMonitor::getTopicStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<TopicStats> result;
    result.reserve(topic_stats_.size());
    
    for (const auto& [name, stats] : topic_stats_) {
        result.push_back(stats);
    }
    
    return result;
}

SystemStats SystemMonitor::getSystemStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SystemStats stats;
    stats.total_nodes = nodes_.size();
    
    for (const auto& [id, node] : nodes_) {
        if (node.status == NodeStatus::RUNNING) {
            stats.running_nodes++;
        }
        stats.total_messages_sent += node.cpu_usage_percent;  // 占位
    }
    
    stats.total_topics = topic_stats_.size();
    stats.total_channels = channel_stats_.size();
    
    for (const auto& [name, ch_stats] : channel_stats_) {
        stats.total_messages_sent += ch_stats.messages_sent;
        stats.total_messages_received += ch_stats.messages_received;
        stats.total_bytes_transferred += ch_stats.bytes_sent + ch_stats.bytes_received;
    }
    
    // 系统资源（简化实现）
    stats.system_cpu_usage = 0;
    stats.system_memory_used = 0;
    stats.system_memory_total = 8ULL * 1024 * 1024 * 1024;  // 假设8GB
    
    return stats;
}

std::vector<NodeInfo> SystemMonitor::checkHealth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<NodeInfo> unhealthy;
    uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    for (const auto& [id, node] : nodes_) {
        if (node.status != NodeStatus::RUNNING) {
            unhealthy.push_back(node);
            continue;
        }
        
        // 检查心跳超时
        if (current_time - node.last_heartbeat > config_.heartbeat_timeout_ms) {
            NodeInfo timeout_node = node;
            timeout_node.status = NodeStatus::TIMEOUT;
            unhealthy.push_back(timeout_node);
        }
    }
    
    return unhealthy;
}

std::vector<NodeInfo> SystemMonitor::getTimeoutNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<NodeInfo> timeout_nodes;
    uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    for (const auto& [id, node] : nodes_) {
        if (node.status == NodeStatus::RUNNING && 
            current_time - node.last_heartbeat > config_.heartbeat_timeout_ms) {
            NodeInfo timeout_node = node;
            timeout_node.status = NodeStatus::TIMEOUT;
            timeout_nodes.push_back(timeout_node);
        }
    }
    
    return timeout_nodes;
}

} // namespace lwipc
