#include "service_discovery.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <sstream>

namespace lwipc {

ServiceDiscovery::ServiceDiscovery(const std::string& endpoint)
    : endpoint_(endpoint) {
}

ServiceDiscovery::~ServiceDiscovery() {
    Stop();
}

bool ServiceDiscovery::RegisterService(const ServiceInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto key = info.channel_name;
    services_[key].push_back(info);
    
    // Broadcast announcement
    if (running_) {
        SendAnnouncement(info);
    }
    
    return true;
}

bool ServiceDiscovery::UnregisterService(const std::string& node_name, 
                                          const std::string& channel_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = services_.find(channel_name);
    if (it == services_.end()) {
        return false;
    }
    
    auto& services = it->second;
    services.erase(
        std::remove_if(services.begin(), services.end(),
            [&node_name](const ServiceInfo& info) {
                return info.node_name == node_name;
            }),
        services.end()
    );
    
    if (services.empty()) {
        services_.erase(it);
    }
    
    return true;
}

std::vector<ServiceInfo> ServiceDiscovery::FindServices(const std::string& channel_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = services_.find(channel_name);
    if (it != services_.end()) {
        return it->second;
    }
    return {};
}

std::vector<ServiceInfo> ServiceDiscovery::GetAllServices() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ServiceInfo> all;
    for (const auto& [key, services] : services_) {
        all.insert(all.end(), services.begin(), services.end());
    }
    return all;
}

void ServiceDiscovery::OnServiceFound(ServiceCallback callback) {
    on_found_ = std::move(callback);
}

void ServiceDiscovery::OnServiceLost(ServiceCallback callback) {
    on_lost_ = std::move(callback);
}

bool ServiceDiscovery::Start() {
    if (running_) {
        return false;
    }
    
    // Create UDP socket for multicast
    udp_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket_ < 0) {
        return false;
    }
    
    // Allow address reuse
    int opt = 1;
    setsockopt(udp_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Parse endpoint
    size_t colon_pos = endpoint_.find_last_of(':');
    std::string ip = endpoint_.substr(0, colon_pos);
    int port = std::stoi(endpoint_.substr(colon_pos + 1));
    
    // Bind to multicast address
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    
    if (bind(udp_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(udp_socket_);
        udp_socket_ = -1;
        return false;
    }
    
    // Join multicast group
    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(ip.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    
    if (setsockopt(udp_socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        close(udp_socket_);
        udp_socket_ = -1;
        return false;
    }
    
    running_ = true;
    stop_flag_ = false;
    
    // Start listener thread
    listen_thread_ = std::thread(&ServiceDiscovery::ListenForAnnouncements, this);
    
    return true;
}

void ServiceDiscovery::Stop() {
    if (!running_) {
        return;
    }
    
    stop_flag_ = true;
    running_ = false;
    
    if (listen_thread_.joinable()) {
        listen_thread_.join();
    }
    
    if (udp_socket_ >= 0) {
        // Leave multicast group
        size_t colon_pos = endpoint_.find_last_of(':');
        std::string ip = endpoint_.substr(0, colon_pos);
        
        struct ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = inet_addr(ip.c_str());
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt(udp_socket_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
        
        close(udp_socket_);
        udp_socket_ = -1;
    }
}

void ServiceDiscovery::SendAnnouncement(const ServiceInfo& info) {
    if (udp_socket_ < 0) {
        return;
    }
    
    // Serialize service info (simple format for demo)
    std::ostringstream oss;
    oss << info.node_name << "|" 
        << info.channel_name << "|" 
        << info.type_name << "|" 
        << (info.is_transmitter ? "TX" : "RX") << "|" 
        << info.timestamp;
    
    std::string message = oss.str();
    
    // Parse endpoint
    size_t colon_pos = endpoint_.find_last_of(':');
    std::string ip = endpoint_.substr(0, colon_pos);
    int port = std::stoi(endpoint_.substr(colon_pos + 1));
    
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    
    sendto(udp_socket_, message.c_str(), message.size(), 0,
           (struct sockaddr*)&addr, sizeof(addr));
}

void ServiceDiscovery::ListenForAnnouncements() {
    constexpr size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    
    while (!stop_flag_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(udp_socket_, &fds);
        
        struct timeval timeout{1, 0};  // 1 second timeout
        
        int ret = select(udp_socket_ + 1, &fds, nullptr, nullptr, &timeout);
        if (ret <= 0) {
            continue;
        }
        
        ssize_t n = recv(udp_socket_, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) {
            continue;
        }
        
        buffer[n] = '\0';
        
        // Parse announcement
        std::istringstream iss(buffer);
        std::string token;
        ServiceInfo info;
        
        if (std::getline(iss, token, '|')) info.node_name = token;
        if (std::getline(iss, token, '|')) info.channel_name = token;
        if (std::getline(iss, token, '|')) info.type_name = token;
        if (std::getline(iss, token, '|')) info.is_transmitter = (token == "TX");
        if (std::getline(iss, token, '|')) info.timestamp = std::stoll(token);
        
        ProcessAnnouncement(info);
    }
}

void ServiceDiscovery::ProcessAnnouncement(const ServiceInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if service already exists
    auto& services = services_[info.channel_name];
    bool found = false;
    
    for (auto& existing : services) {
        if (existing.node_name == info.node_name && 
            existing.channel_name == info.channel_name) {
            existing.timestamp = info.timestamp;  // Update timestamp
            found = true;
            break;
        }
    }
    
    if (!found) {
        services.push_back(info);
        if (on_found_) {
            on_found_(info);
        }
    }
}

void ServiceDiscovery::CleanupExpiredServices() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    constexpr int64_t EXPIRY_SECONDS = 30;
    
    for (auto it = services_.begin(); it != services_.end();) {
        auto& services = it->second;
        services.erase(
            std::remove_if(services.begin(), services.end(),
                [now](const ServiceInfo& info) {
                    return (now - info.timestamp) > EXPIRY_SECONDS;
                }),
            services.end()
        );
        
        if (services.empty()) {
            it = services_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace lwipc
