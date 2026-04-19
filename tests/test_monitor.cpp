#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include "lwipc/system_monitor.hpp"

using namespace lwipc;

void test_node_registration() {
    std::cout << "Testing node registration... ";
    
    SystemMonitor monitor;
    
    // 初始为空
    assert(monitor.getNodes().empty());
    
    // 注册节点
    NodeInfo node1;
    node1.id = "node_1";
    node1.name = "SensorNode";
    node1.host = "localhost";
    node1.pid = 12345;
    node1.status = NodeStatus::RUNNING;
    
    monitor.registerNode(node1);
    assert(monitor.getNodes().size() == 1);
    
    NodeInfo node2;
    node2.id = "node_2";
    node2.name = "ControlNode";
    node2.host = "localhost";
    node2.pid = 12346;
    node2.status = NodeStatus::RUNNING;
    
    monitor.registerNode(node2);
    assert(monitor.getNodes().size() == 2);
    
    // 获取特定节点
    auto retrieved = monitor.getNode("node_1");
    assert(retrieved.id == "node_1");
    assert(retrieved.name == "SensorNode");
    
    // 获取不存在的节点
    auto not_found = monitor.getNode("nonexistent");
    assert(not_found.id.empty());
    
    // 注销节点
    monitor.unregisterNode("node_2");
    assert(monitor.getNodes().size() == 1);
    
    std::cout << "PASSED" << std::endl;
}

void test_heartbeat() {
    std::cout << "Testing heartbeat... ";
    
    SystemMonitor monitor;
    
    SystemMonitor::Config config;
    config.heartbeat_timeout_ms = 1000;  // 1秒超时
    monitor.configure(config);
    
    NodeInfo node;
    node.id = "test_node";
    node.name = "TestNode";
    node.status = NodeStatus::RUNNING;
    
    monitor.registerNode(node);
    monitor.updateHeartbeat("test_node");
    
    // 立即检查，应该没有超时节点
    auto timeout_nodes = monitor.getTimeoutNodes();
    assert(timeout_nodes.empty());
    
    // 等待超过超时时间
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    timeout_nodes = monitor.getTimeoutNodes();
    assert(timeout_nodes.size() == 1);
    assert(timeout_nodes[0].status == NodeStatus::TIMEOUT);
    
    // 更新心跳后恢复正常
    monitor.updateHeartbeat("test_node");
    timeout_nodes = monitor.getTimeoutNodes();
    assert(timeout_nodes.empty());
    
    std::cout << "PASSED" << std::endl;
}

void test_channel_stats() {
    std::cout << "Testing channel statistics... ";
    
    SystemMonitor monitor;
    
    // 记录消息发送
    monitor.recordMessageSent("channel_1", 100, 50.0);  // 100字节，50微秒延迟
    monitor.recordMessageSent("channel_1", 200, 60.0);
    monitor.recordMessageSent("channel_1", 150, 40.0);
    
    // 记录消息接收
    monitor.recordMessageReceived("channel_1", 100);
    monitor.recordMessageReceived("channel_1", 200);
    
    // 记录丢弃
    monitor.recordMessageDropped("channel_1");
    
    auto stats = monitor.getChannelStats();
    assert(stats.size() == 1);
    
    const auto& ch_stats = stats[0];
    assert(ch_stats.name == "channel_1");
    assert(ch_stats.messages_sent == 3);
    assert(ch_stats.messages_received == 2);
    assert(ch_stats.bytes_sent == 450);
    assert(ch_stats.messages_dropped == 1);
    assert(ch_stats.avg_latency_us > 0);
    assert(ch_stats.min_latency_us == 40.0);
    assert(ch_stats.max_latency_us == 60.0);
    
    std::cout << "PASSED" << std::endl;
}

void test_topic_stats() {
    std::cout << "Testing topic statistics... ";
    
    SystemMonitor monitor;
    
    monitor.updateTopicInfo("/sensors/imu", "sensor_msgs/Imu", 48);
    monitor.updateTopicInfo("/sensors/imu", "sensor_msgs/Imu", 48);
    monitor.updateTopicInfo("/sensors/imu", "sensor_msgs/Imu", 48);
    
    auto stats = monitor.getTopicStats();
    assert(stats.size() == 1);
    
    const auto& topic_stats = stats[0];
    assert(topic_stats.name == "/sensors/imu");
    assert(topic_stats.type == "sensor_msgs/Imu");
    assert(topic_stats.message_count == 3);
    assert(topic_stats.avg_msg_size_bytes == 48);
    
    std::cout << "PASSED" << std::endl;
}

void test_system_stats() {
    std::cout << "Testing system statistics... ";
    
    SystemMonitor monitor;
    
    // 注册多个节点
    for (int i = 0; i < 5; ++i) {
        NodeInfo node;
        node.id = "node_" + std::to_string(i);
        node.name = "Node" + std::to_string(i);
        node.status = NodeStatus::RUNNING;
        monitor.registerNode(node);
    }
    
    // 添加一些通道统计
    monitor.recordMessageSent("ch1", 1000, 10.0);
    monitor.recordMessageReceived("ch1", 1000);
    monitor.recordMessageSent("ch2", 2000, 20.0);
    monitor.recordMessageReceived("ch2", 2000);
    
    auto sys_stats = monitor.getSystemStats();
    assert(sys_stats.total_nodes == 5);
    assert(sys_stats.running_nodes == 5);
    assert(sys_stats.total_channels == 2);
    assert(sys_stats.total_messages_sent == 2);
    assert(sys_stats.total_messages_received == 2);
    assert(sys_stats.total_bytes_transferred == 6000);
    
    std::cout << "PASSED" << std::endl;
}

void test_health_check() {
    std::cout << "Testing health check... ";
    
    SystemMonitor monitor;
    
    SystemMonitor::Config config;
    config.heartbeat_timeout_ms = 500;
    monitor.configure(config);
    
    // 注册节点但不更新心跳
    NodeInfo node1;
    node1.id = "healthy_node";
    node1.status = NodeStatus::RUNNING;
    monitor.registerNode(node1);
    monitor.updateHeartbeat("healthy_node");
    
    NodeInfo node2;
    node2.id = "unhealthy_node";
    node2.status = NodeStatus::RUNNING;
    monitor.registerNode(node2);
    // 不更新心跳
    
    // 等待超时
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    auto unhealthy = monitor.checkHealth();
    assert(unhealthy.size() == 1);
    assert(unhealthy[0].id == "unhealthy_node");
    
    std::cout << "PASSED" << std::endl;
}

void test_alert_callback() {
    std::cout << "Testing alert callback... ";
    
    SystemMonitor monitor;
    
    int alert_count = 0;
    monitor.setAlertCallback([&alert_count](const Alert& alert) {
        alert_count++;
    });
    
    SystemMonitor::Config config;
    config.enable_alerts = true;
    config.cpu_threshold = 50.0;  // 设置低阈值以便触发
    config.latency_threshold_us = 10.0;
    monitor.configure(config);
    monitor.start();
    
    // 注册节点并更新高CPU使用率
    NodeInfo node;
    node.id = "high_cpu_node";
    node.status = NodeStatus::RUNNING;
    monitor.registerNode(node);
    monitor.updateNodeResources("high_cpu_node", 95.0, 1024 * 1024 * 100);
    
    // 记录高延迟消息
    monitor.recordMessageSent("test_channel", 100, 100.0);
    
    // 应该有告警触发
    assert(alert_count > 0);
    
    monitor.stop();
    
    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "=== System Monitor Tests ===" << std::endl;
    
    test_node_registration();
    test_heartbeat();
    test_channel_stats();
    test_topic_stats();
    test_system_stats();
    test_health_check();
    test_alert_callback();
    
    std::cout << "\nAll tests PASSED!" << std::endl;
    return 0;
}
