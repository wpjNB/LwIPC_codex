/**
 * @file example_network_bridge.cpp
 * @brief 网络通信桥接测试示例
 * 
 * 功能：模拟跨主机通信场景
 *   - 本地进程通过共享内存发送数据
 *   - 桥接进程接收后通过UDP转发到远程地址
 *   - 远程进程接收并验证数据完整性
 * 
 * 启动方式：
 *   终端1 (Sender):   ./example_network_bridge sender
 *   终端2 (Bridge):   ./example_network_bridge bridge <remote_ip> <port>
 *   终端3 (Receiver): ./example_network_bridge receiver <port>
 */

#include <iostream>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>
#include <string>

constexpr size_t PACKET_SIZE = 1024;
constexpr int NUM_PACKETS = 10000;

volatile sig_atomic_t g_running = 1;

void signal_handler(int sig) {
    g_running = 0;
}

// 数据包结构
struct DataPacket {
    uint32_t sequence;
    uint64_t timestamp;
    uint32_t payload_size;
    char payload[PACKET_SIZE - sizeof(uint32_t) * 2 - sizeof(uint64_t)];
};

int run_sender() {
    std::cout << "[Sender] Starting..." << std::endl;
    
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket == -1) {
        perror("socket failed");
        return 1;
    }
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    std::cout << "[Sender] Sending " << NUM_PACKETS << " packets to 127.0.0.1:9000" << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    int sent = 0;
    
    for (int i = 0; i < NUM_PACKETS && g_running; ++i) {
        DataPacket packet{};
        packet.sequence = i;
        packet.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        packet.payload_size = snprintf(packet.payload, sizeof(packet.payload), 
                                       "Test data packet #%d", i);
        
        ssize_t result = sendto(udp_socket, &packet, sizeof(DataPacket), 0,
                               reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
        
        if (result == -1) {
            perror("sendto failed");
            break;
        }
        
        sent++;
        
        if (i % 1000 == 0) {
            std::cout << "[Sender] Sent " << i << " packets" << std::endl;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    double throughput = (sent * 1000.0) / duration;
    double bandwidth = (sent * sizeof(DataPacket) / 1024.0 / 1024.0) * 1000.0 / duration;
    
    std::cout << "\n=== Sender Results ===" << std::endl;
    std::cout << "Packets sent: " << sent << std::endl;
    std::cout << "Duration: " << duration << " ms" << std::endl;
    std::cout << "Throughput: " << throughput << " pkt/s" << std::endl;
    std::cout << "Bandwidth: " << bandwidth << " MB/s" << std::endl;
    
    close(udp_socket);
    return 0;
}

int run_bridge(const std::string& remote_ip, int port) {
    std::cout << "[Bridge] Starting... Forwarding to " << remote_ip << ":" << (port + 1) << std::endl;
    
    // 接收 socket
    int recv_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_socket == -1) {
        perror("socket failed");
        return 1;
    }
    
    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(recv_socket, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)) == -1) {
        perror("bind failed");
        return 1;
    }
    
    // 转发 socket
    int fwd_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (fwd_socket == -1) {
        perror("socket failed");
        return 1;
    }
    
    sockaddr_in remote_addr{};
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(port + 1);
    inet_pton(AF_INET, remote_ip.c_str(), &remote_addr.sin_addr);
    
    std::cout << "[Bridge] Listening on port " << port << ", forwarding to " 
              << remote_ip << ":" << (port + 1) << std::endl;
    
    int forwarded = 0;
    char buffer[2048];
    
    while (g_running) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        
        ssize_t recv_len = recvfrom(recv_socket, buffer, sizeof(buffer), 0,
                                   reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        
        if (recv_len == -1) {
            if (errno == EINTR) continue;
            perror("recvfrom failed");
            break;
        }
        
        // 转发到远程
        ssize_t fwd_len = sendto(fwd_socket, buffer, recv_len, 0,
                                reinterpret_cast<sockaddr*>(&remote_addr), sizeof(remote_addr));
        
        if (fwd_len == -1) {
            perror("sendto failed");
            continue;
        }
        
        forwarded++;
        
        if (forwarded % 1000 == 0) {
            std::cout << "[Bridge] Forwarded " << forwarded << " packets" << std::endl;
        }
    }
    
    std::cout << "\n=== Bridge Results ===" << std::endl;
    std::cout << "Total forwarded: " << forwarded << std::endl;
    
    close(recv_socket);
    close(fwd_socket);
    return 0;
}

int run_receiver(int port) {
    std::cout << "[Receiver] Starting... Listening on port " << (port + 1) << std::endl;
    
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket == -1) {
        perror("socket failed");
        return 1;
    }
    
    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port + 1);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(udp_socket, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)) == -1) {
        perror("bind failed");
        return 1;
    }
    
    std::cout << "[Receiver] Waiting for packets..." << std::endl;
    
    int received = 0;
    int errors = 0;
    uint32_t last_seq = 0;
    uint64_t total_latency = 0;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    while (g_running) {
        DataPacket packet{};
        sockaddr_in sender_addr{};
        socklen_t addr_len = sizeof(sender_addr);
        
        ssize_t recv_len = recvfrom(udp_socket, &packet, sizeof(packet), 0,
                                   reinterpret_cast<sockaddr*>(&sender_addr), &addr_len);
        
        if (recv_len == -1) {
            if (errno == EINTR) continue;
            perror("recvfrom failed");
            break;
        }
        
        // 验证数据
        if (packet.sequence != last_seq + 1 && received > 0) {
            std::cerr << "[Receiver] Warning: Sequence mismatch. Expected " 
                      << (last_seq + 1) << ", got " << packet.sequence << std::endl;
            errors++;
        }
        last_seq = packet.sequence;
        
        // 计算延迟
        uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        total_latency += (now - packet.timestamp);
        
        received++;
        
        if (received % 1000 == 0) {
            std::cout << "[Receiver] Received " << received << " packets (errors: " << errors << ")" << std::endl;
        }
        
        if (received >= NUM_PACKETS) {
            break;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    double throughput = (received * 1000.0) / duration;
    double bandwidth = (received * sizeof(DataPacket) / 1024.0 / 1024.0) * 1000.0 / duration;
    double avg_latency = static_cast<double>(total_latency) / received;
    
    std::cout << "\n=== Receiver Results ===" << std::endl;
    std::cout << "Packets received: " << received << std::endl;
    std::cout << "Errors: " << errors << std::endl;
    std::cout << "Duration: " << duration << " ms" << std::endl;
    std::cout << "Throughput: " << throughput << " pkt/s" << std::endl;
    std::cout << "Bandwidth: " << bandwidth << " MB/s" << std::endl;
    std::cout << "Avg Latency: " << avg_latency << " us" << std::endl;
    
    close(udp_socket);
    return 0;
}

void print_usage(const char* prog) {
    std::cerr << "Usage:" << std::endl;
    std::cerr << "  " << prog << " sender                    # Send test packets" << std::endl;
    std::cerr << "  " << prog << " bridge <ip> <port>        # Bridge packets to remote" << std::endl;
    std::cerr << "  " << prog << " receiver <port>           # Receive forwarded packets" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string role = argv[1];
    
    if (role == "sender") {
        return run_sender();
    } else if (role == "bridge") {
        if (argc != 4) {
            std::cerr << "Bridge requires IP and port arguments" << std::endl;
            print_usage(argv[0]);
            return 1;
        }
        return run_bridge(argv[2], std::stoi(argv[3]));
    } else if (role == "receiver") {
        if (argc != 3) {
            std::cerr << "Receiver requires port argument" << std::endl;
            print_usage(argv[0]);
            return 1;
        }
        return run_receiver(std::stoi(argv[2]));
    } else {
        std::cerr << "Invalid role: " << role << std::endl;
        print_usage(argv[0]);
        return 1;
    }
}
