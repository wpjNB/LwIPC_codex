#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <atomic>

namespace lwipc {

/**
 * @brief Service information structure for discovery
 */
struct ServiceInfo {
    std::string node_name;
    std::string channel_name;
    std::string type_name;
    std::string endpoint;  // IP:Port or shm_path
    bool is_transmitter;
    int64_t timestamp;
    
    ServiceInfo() : is_transmitter(false), timestamp(0) {}
};

/**
 * @brief ServiceDiscovery - Dynamic service discovery mechanism
 * 
 * Inspired by FastDDS Discovery Protocol and CyberRT Service Discovery.
 * Uses UDP multicast for network discovery and shared memory for local discovery.
 */
class ServiceDiscovery {
public:
    using ServiceCallback = std::function<void(const ServiceInfo&)>;
    using ServiceMap = std::unordered_map<std::string, std::vector<ServiceInfo>>;

    explicit ServiceDiscovery(const std::string& endpoint = "239.255.0.1:8888");
    ~ServiceDiscovery();

    // Register/unregister services
    bool RegisterService(const ServiceInfo& info);
    bool UnregisterService(const std::string& node_name, const std::string& channel_name);

    // Query services
    std::vector<ServiceInfo> FindServices(const std::string& channel_name);
    std::vector<ServiceInfo> GetAllServices();
    
    // Subscribe to discovery events
    void OnServiceFound(ServiceCallback callback);
    void OnServiceLost(ServiceCallback callback);

    // Lifecycle
    bool Start();
    void Stop();
    bool IsRunning() const { return running_; }

private:
    void SendAnnouncement(const ServiceInfo& info);
    void ListenForAnnouncements();
    void ProcessAnnouncement(const ServiceInfo& info);
    void CleanupExpiredServices();

    std::string endpoint_;
    ServiceMap services_;
    mutable std::mutex mutex_;
    
    ServiceCallback on_found_;
    ServiceCallback on_lost_;
    
    bool running_{false};
    int udp_socket_{-1};
    
    // Thread for listening
    std::thread listen_thread_;
    std::atomic<bool> stop_flag_{false};
};

} // namespace lwipc
