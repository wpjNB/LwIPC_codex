/**
 * @file example_monitor.cpp
 * @brief 系统监控示例
 * 
 * 演示如何使用 LwIPC 的系统监控功能
 */

#include <lwipc/system_monitor.hpp>
#include <lwipc/broker.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace lwipc;

int main() {
    auto& monitor = SystemMonitor::instance();
    
    // 启动监控
    std::cout << "=== Starting System Monitor ===" << std::endl;
    monitor.start();
    
    // 注册模拟节点
    std::cout << "\n=== Registering Nodes ===" << std::endl;
    monitor.registerNode("node_lidar", NodeType::SENSOR);
    monitor.registerNode("node_planner", NodeType::COMPUTE);
    monitor.registerNode("node_control", NodeType::ACTUATOR);
    monitor.registerNode("node_logger", NodeType::UTILITY);
    
    // 模拟节点活动
    std::cout << "\n=== Simulating Node Activity ===" << std::endl;
    
    for (int i = 0; i < 5; ++i) {
        // 更新节点心跳
        monitor.updateHeartbeat("node_lidar");
        monitor.updateHeartbeat("node_planner");
        monitor.updateHeartbeat("node_control");
        
        // 记录性能指标
        Metrics latency_metrics;
        latency_metrics.avg_latency_us = 150 + i * 10;
        latency_metrics.max_latency_us = 300 + i * 20;
        latency_metrics.min_latency_us = 80 + i * 5;
        monitor.recordMetrics("node_lidar", "publish_latency", latency_metrics);
        
        Metrics throughput_metrics;
        throughput_metrics.messages_per_second = 1000 + i * 100;
        throughput_metrics.bytes_per_second = 1024 * 1024 * (10 + i);
        monitor.recordMetrics("node_planner", "throughput", throughput_metrics);
        
        std::cout << "[Iteration " << i << "] Heartbeats updated, metrics recorded" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    // 获取节点状态
    std::cout << "\n=== Node Status ===" << std::endl;
    auto nodes = monitor.getNodeStatus();
    for (const auto& node : nodes) {
        std::cout << "Node: " << node.name 
                  << ", Type: " << static_cast<int>(node.type)
                  << ", State: " << (node.state == NodeState::HEALTHY ? "HEALTHY" : "UNKNOWN")
                  << ", Last heartbeat: " << node.last_heartbeat << std::endl;
    }
    
    // 获取性能指标
    std::cout << "\n=== Performance Metrics ===" << std::endl;
    
    auto lidar_metrics = monitor.getMetrics("node_lidar", "publish_latency");
    if (lidar_metrics) {
        std::cout << "LiDAR Publish Latency:" << std::endl;
        std::cout << "  Avg: " << lidar_metrics->avg_latency_us << " us" << std::endl;
        std::cout << "  Max: " << lidar_metrics->max_latency_us << " us" << std::endl;
        std::cout << "  Min: " << lidar_metrics->min_latency_us << " us" << std::endl;
    }
    
    auto planner_metrics = monitor.getMetrics("node_planner", "throughput");
    if (planner_metrics) {
        std::cout << "\nPlanner Throughput:" << std::endl;
        std::cout << "  Messages/sec: " << planner_metrics->messages_per_second << std::endl;
        std::cout << "  Bytes/sec: " << planner_metrics->bytes_per_second << std::endl;
    }
    
    // 获取系统统计信息
    std::cout << "\n=== System Statistics ===" << std::endl;
    auto stats = monitor.getSystemStats();
    std::cout << "Total nodes: " << stats.total_nodes << std::endl;
    std::cout << "Healthy nodes: " << stats.healthy_nodes << std::endl;
    std::cout << "Unhealthy nodes: " << stats.unhealthy_nodes << std::endl;
    std::cout << "Total messages: " << stats.total_messages << std::endl;
    std::cout << "Avg system latency: " << stats.avg_latency_us << " us" << std::endl;
    
    // 模拟节点故障
    std::cout << "\n=== Simulating Node Failure ===" << std::endl;
    std::cout << "Stopping heartbeat updates for node_control..." << std::endl;
    
    // 等待一段时间让节点变为不健康
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    // 再次检查状态
    auto updated_nodes = monitor.getNodeStatus();
    for (const auto& node : updated_nodes) {
        if (node.name == "node_control") {
            std::cout << "Node: " << node.name 
                      << ", State: " << (node.state == NodeState::UNHEALTHY ? "UNHEALTHY" : "HEALTHY")
                      << std::endl;
        }
    }
    
    // 停止监控
    std::cout << "\n=== Stopping Monitor ===" << std::endl;
    monitor.stop();
    
    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}
