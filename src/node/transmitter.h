#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <vector>

#include "../include/lwipc/message.hpp"

namespace lwipc {

/**
 * @brief Transmitter - Abstract base class for sending messages
 * 
 * Similar to CyberRT Writer and FastDDS DataWriter.
 * Supports multiple transport protocols (shared memory, network).
 */
class Transmitter {
public:
    explicit Transmitter(const std::string& channel, const QoSProfile& qos = QoSProfile());
    virtual ~Transmitter();

    // Send message
    template<typename MessageType>
    bool Send(const MessageType& msg);
    
    bool SendRaw(const uint8_t* data, size_t size);

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
    bool initialized_{false};
};

template<typename MessageType>
bool Transmitter::Send(const MessageType& msg) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&msg);
    size_t size = sizeof(MessageType);
    return SendRaw(data, size);
}

} // namespace lwipc
