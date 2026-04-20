#include "transmitter.h"
#include "../transport/shm_channel.h"
#include <cstring>

namespace lwipc {

Transmitter::Transmitter(const std::string& channel, const QoSProfile& qos)
    : channel_(channel)
    , qos_(qos) {
}

Transmitter::~Transmitter() {
    Shutdown();
}

bool Transmitter::Init() {
    if (initialized_) {
        return true;
    }
    
    // Initialize underlying transport (e.g., shared memory)
    // In a full implementation, this would select the appropriate transport
    // based on QoS requirements and destination
    initialized_ = true;
    return true;
}

void Transmitter::Shutdown() {
    if (!initialized_) {
        return;
    }
    
    // Cleanup resources
    initialized_ = false;
}

bool Transmitter::SendRaw(const uint8_t* data, size_t size) {
    if (!initialized_) {
        return false;
    }
    
    // For now, use shared memory as default transport
    // In production, this would route based on destination and QoS
    ShmChannel shm;
    std::string shm_key = "/lwipc_" + channel_;
    
    if (!shm.IsInitialized()) {
        shm.InitializeCreator(shm_key.c_str(), size * 10, 10);
    }
    
    auto buffer = shm.Allocate(size);
    if (!buffer) {
        return false;
    }
    
    std::memcpy(buffer, data, size);
    shm.Commit();
    
    return true;
}

} // namespace lwipc
