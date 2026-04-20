#include "node.h"
#include <iostream>

namespace lwipc {

Node::Node(const NodeOptions& options)
    : name_(options.node_name)
    , namespace_(options.namespace_)
    , options_(options) {
    
    if (!namespace_.empty() && namespace_.back() == '/') {
        namespace_.pop_back();
    }
    
    full_name_ = namespace_.empty() ? name_ : namespace_ + "/" + name_;
    
    // Initialize service discovery
    if (options.enable_service_discovery) {
        discovery_ = std::make_unique<ServiceDiscovery>(options.discovery_endpoint);
    }
    
    std::cout << "[Node] Created: " << full_name_ << std::endl;
}

Node::~Node() {
    Stop();
    std::cout << "[Node] Destroyed: " << full_name_ << std::endl;
}

bool Node::Start() {
    if (running_.exchange(true)) {
        return false;  // Already running
    }
    
    // Start all transmitters
    for (auto& [name, tx] : transmitters_) {
        tx->Init();
    }
    
    // Start all receivers
    for (auto& [name, rx] : receivers_) {
        rx->Init();
    }
    
    // Start service discovery
    if (discovery_) {
        discovery_->Start();
    }
    
    std::cout << "[Node] Started: " << full_name_ << std::endl;
    return true;
}

bool Node::Stop() {
    if (!running_.exchange(false)) {
        return false;  // Not running
    }
    
    // Stop service discovery
    if (discovery_) {
        discovery_->Stop();
    }
    
    // Shutdown all receivers
    for (auto& [name, rx] : receivers_) {
        rx->Shutdown();
    }
    
    // Shutdown all transmitters
    for (auto& [name, tx] : transmitters_) {
        tx->Shutdown();
    }
    
    std::cout << "[Node] Stopped: " << full_name_ << std::endl;
    return true;
}

void Node::OnServiceFound(DiscoveryCallback callback) {
    if (discovery_) {
        discovery_->OnServiceFound(callback);
    }
}

void Node::OnServiceLost(DiscoveryCallback callback) {
    if (discovery_) {
        discovery_->OnServiceLost(callback);
    }
}

} // namespace lwipc
