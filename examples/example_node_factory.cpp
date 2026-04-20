#include "node/node.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

// Example message structure
struct PoseMessage {
    double x, y, z;
    double qx, qy, qz, qw;
    uint64_t timestamp;
};

int main() {
    using namespace lwipc;
    
    std::cout << "=== LwIPC Node Factory Example ===" << std::endl;
    
    // Create publisher node
    NodeOptions pub_options;
    pub_options.node_name = "pose_publisher";
    pub_options.namespace_ = "/sensors";
    pub_options.qos_profile.reliability = ReliabilityPolicy::RELIABLE;
    
    auto pub_node = std::make_shared<Node>(pub_options);
    
    // Create subscriber node
    NodeOptions sub_options;
    sub_options.node_name = "pose_subscriber";
    sub_options.namespace_ = "/sensors";
    sub_options.qos_profile.reliability = ReliabilityPolicy::BEST_EFFORT;
    
    auto sub_node = std::make_shared<Node>(sub_options);
    
    // Setup service discovery callbacks
    sub_node->OnServiceFound([](const ServiceInfo& info) {
        std::cout << "[Discovery] Service found: " 
                  << info.node_name << " -> " << info.channel_name 
                  << " (Type: " << info.type_name << ")" << std::endl;
    });
    
    // Create transmitter using factory pattern
    auto transmitter = pub_node->CreateTransmitter<PoseMessage>("pose");
    
    // Create receiver using factory pattern with callback
    std::atomic<int> messages_received{0};
    
    auto receiver = sub_node->CreateReceiver<PoseMessage>("pose",
        [&messages_received](const PoseMessage& msg) {
            messages_received++;
            std::cout << "[Subscriber] Received pose: (" 
                      << msg.x << ", " << msg.y << ", " << msg.z << ") "
                      << "(Total: " << messages_received.load() << ")" << std::endl;
        });
    
    // Start both nodes
    pub_node->Start();
    sub_node->Start();
    
    std::cout << "\nNodes started. Publishing messages..." << std::endl;
    
    // Publish messages
    PoseMessage pose{};
    for (int i = 0; i < 10; ++i) {
        pose.x = i * 1.0;
        pose.y = i * 2.0;
        pose.z = i * 3.0;
        pose.qw = 1.0;
        pose.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        transmitter->Send(pose);
        std::cout << "[Publisher] Sent pose #" << (i + 1) << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Wait for final messages to be processed
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Query discovered services
    std::cout << "\n=== Discovered Services ===" << std::endl;
    auto services = sub_node->GetDiscovery()->GetAllServices();
    for (const auto& svc : services) {
        std::cout << "  - " << svc.node_name << "/" << svc.channel_name 
                  << " [" << (svc.is_transmitter ? "TX" : "RX") << "]" << std::endl;
    }
    
    // Stop nodes
    pub_node->Stop();
    sub_node->Stop();
    
    std::cout << "\nExample completed. Total messages received: " 
              << messages_received.load() << std::endl;
    
    return 0;
}
