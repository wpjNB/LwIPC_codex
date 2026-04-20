#include "receiver.h"
#include "../transport/shm_channel.h"
#include <cstring>
#include <thread>
#include <chrono>

namespace lwipc {

Receiver::Receiver(const std::string& channel, const QoSProfile& qos)
    : channel_(channel)
    , qos_(qos) {
}

Receiver::~Receiver() {
    Shutdown();
}

bool Receiver::Init() {
    if (initialized_) {
        return true;
    }
    
    // Initialize underlying transport
    initialized_ = true;
    return true;
}

void Receiver::Shutdown() {
    if (!initialized_) {
        return;
    }
    
    initialized_ = false;
}

void Receiver::SetCallback(Callback callback) {
    callback_ = std::move(callback);
    
    // Start a background thread to poll for messages when callback is set
    if (callback_ && initialized_) {
        std::thread([this]() {
            constexpr size_t MAX_MSG_SIZE = 65536;
            std::vector<uint8_t> buffer(MAX_MSG_SIZE);
            
            while (initialized_) {
                size_t size = 0;
                if (Poll(buffer.data(), size, 100)) {
                    if (callback_ && size > 0) {
                        callback_(buffer.data(), size);
                    }
                }
            }
        }).detach();
    }
}

bool Receiver::Poll(uint8_t* buffer, size_t& size, int timeout_ms) {
    if (!initialized_) {
        return false;
    }
    
    ShmChannel shm;
    std::string shm_key = "/lwipc_" + channel_;
    
    if (!shm.IsInitialized()) {
        if (!shm.InitializeConsumer(shm_key.c_str())) {
            return false;
        }
    }
    
    auto view = shm.ReadLatest();
    if (!view) {
        return false;
    }
    
    size = view->size;
    if (buffer && size > 0) {
        std::memcpy(buffer, view->data, std::min(size, MAX_MSG_SIZE));
    }
    
    return true;
}

} // namespace lwipc
