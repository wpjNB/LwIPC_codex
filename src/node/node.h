#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

#include "transmitter.h"
#include "receiver.h"
#include "service_discovery.h"
#include "../include/lwipc/message.hpp"
#include "../include/lwipc/transport.hpp"

namespace lwipc {

/**
 * @brief Node configuration structure
 */
struct NodeOptions {
    std::string node_name;
    std::string namespace_;
    QoSProfile qos_profile;
    bool enable_service_discovery = true;
    std::string discovery_endpoint = "239.255.0.1:8888";
};

/**
 * @brief Node class - Core abstraction for communication entities
 * 
 * Inspired by CyberRT Node and FastDDS Participant.
 * Each node can create multiple transmitters and receivers.
 */
class Node {
public:
    using TransmitterPtr = std::shared_ptr<Transmitter>;
    using ReceiverPtr = std::shared_ptr<Receiver>;
    using DiscoveryCallback = std::function<void(const ServiceInfo&)>;

    explicit Node(const NodeOptions& options);
    ~Node();

    // Factory methods for creating communication entities
    template<typename MessageType>
    TransmitterPtr CreateTransmitter(const std::string& channel_name);
    
    template<typename MessageType>
    ReceiverPtr CreateReceiver(const std::string& channel_name, 
                               std::function<void(const MessageType&)> callback);

    // Service discovery callbacks
    void OnServiceFound(DiscoveryCallback callback);
    void OnServiceLost(DiscoveryCallback callback);

    // Node lifecycle
    bool Start();
    bool Stop();
    bool IsRunning() const { return running_.load(); }

    // Getters
    const std::string& GetName() const { return name_; }
    const std::string& GetNamespace() const { return namespace_; }
    ServiceDiscovery* GetDiscovery() { return discovery_.get(); }

private:
    std::string name_;
    std::string namespace_;
    std::string full_name_;
    NodeOptions options_;
    
    std::unique_ptr<ServiceDiscovery> discovery_;
    
    std::unordered_map<std::string, TransmitterPtr> transmitters_;
    std::unordered_map<std::string, ReceiverPtr> receivers_;
    
    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};
    
    Logger logger_;
};

// Template implementations
template<typename MessageType>
auto Node::CreateTransmitter(const std::string& channel_name) -> TransmitterPtr {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto full_channel = namespace_.empty() ? channel_name : namespace_ + "/" + channel_name;
    
    if (transmitters_.count(full_channel)) {
        std::cout << "[Node] Warning: Transmitter " << full_channel << " already exists" << std::endl;
        return transmitters_[full_channel];
    }
    
    auto transmitter = std::make_shared<Transmitter>(full_channel, options_.qos_profile);
    transmitters_[full_channel] = transmitter;
    
    // Register with service discovery
    if (discovery_ && options_.enable_service_discovery) {
        ServiceInfo info;
        info.node_name = name_;
        info.channel_name = full_channel;
        info.type_name = typeid(MessageType).name();
        info.is_transmitter = true;
        discovery_->RegisterService(info);
    }
    
    std::cout << "[Node] Created transmitter: " << full_channel << std::endl;
    return transmitter;
}

template<typename MessageType>
auto Node::CreateReceiver(const std::string& channel_name,
                          std::function<void(const MessageType&)> callback) -> ReceiverPtr {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto full_channel = namespace_.empty() ? channel_name : namespace_ + "/" + channel_name;
    
    if (receivers_.count(full_channel)) {
        std::cout << "[Node] Warning: Receiver " << full_channel << " already exists" << std::endl;
        return receivers_[full_channel];
    }
    
    auto receiver = std::make_shared<Receiver>(full_channel, options_.qos_profile);
    
    // Wrap callback with type erasure
    receiver->SetCallback([callback](const uint8_t* data, size_t size) {
        if (size >= sizeof(MessageType)) {
            const MessageType& msg = *reinterpret_cast<const MessageType*>(data);
            callback(msg);
        }
    });
    
    receivers_[full_channel] = receiver;
    
    // Register with service discovery
    if (discovery_ && options_.enable_service_discovery) {
        ServiceInfo info;
        info.node_name = name_;
        info.channel_name = full_channel;
        info.type_name = typeid(MessageType).name();
        info.is_transmitter = false;
        discovery_->RegisterService(info);
    }
    
    std::cout << "[Node] Created receiver: " << full_channel << std::endl;
    return receiver;
}

} // namespace lwipc
