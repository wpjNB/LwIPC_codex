#pragma once

#include <string>
#include <cstdint>
#include <functional>
#include <memory>

#include "../include/lwipc/message.hpp"

namespace lwipc {

/**
 * @brief Receiver - Abstract base class for receiving messages
 * 
 * Similar to CyberRT Reader and FastDDS DataReader.
 * Supports callback-based and polling message consumption.
 */
class Receiver {
public:
    using Callback = std::function<void(const uint8_t* data, size_t size)>;

    explicit Receiver(const std::string& channel, const QoSProfile& qos = QoSProfile());
    virtual ~Receiver();

    // Set callback for message reception
    void SetCallback(Callback callback);
    
    // Poll for messages (non-blocking)
    virtual bool Poll(uint8_t* buffer, size_t& size, int timeout_ms = 0);

    // Getters
    const std::string& GetChannel() const { return channel_; }
    const QoSProfile& GetQoS() const { return qos_; }
    
    // Lifecycle
    virtual bool Init();
    virtual void Shutdown();
    bool IsInitialized() const { return initialized_; }

protected:
    std::string channel_;
    QoSProfile qos_;
    Callback callback_;
    bool initialized_{false};
};

} // namespace lwipc
