/**
 * @file example_ipc_stress.cpp
 * @brief 跨进程通信压力测试示例
 * 
 * 启动方式：
 *   进程1 (生产者): ./example_ipc_stress producer
 *   进程2 (消费者): ./example_ipc_stress consumer
 */

#include <iostream>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

// 共享内存配置
constexpr const char* SHM_NAME = "/lwipc_stress_test";
constexpr const char* SEM_PRODUCER = "/lwipc_sem_producer";
constexpr const char* SEM_CONSUMER = "/lwipc_sem_consumer";
constexpr size_t SHM_SIZE = 10 * 1024 * 1024; // 10MB
constexpr size_t MSG_SIZE = 1024; // 1KB per message
constexpr size_t NUM_MESSAGES = 100000;

// 共享内存结构
struct SharedData {
    std::atomic<bool> running{true};
    std::atomic<size_t> write_pos{0};
    std::atomic<size_t> read_pos{0};
    std::atomic<size_t> total_messages{0};
    std::atomic<uint64_t> total_bytes{0};
    std::atomic<uint64_t> start_time{0};
    char buffer[SHM_SIZE];
};

volatile sig_atomic_t g_running = 1;

void signal_handler(int sig) {
    g_running = 0;
}

int run_producer() {
    std::cout << "[Producer] Starting..." << std::endl;
    
    // 创建共享内存
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        return 1;
    }
    
    if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
        perror("ftruncate failed");
        return 1;
    }
    
    SharedData* shared = reinterpret_cast<SharedData*>(
        mmap(nullptr, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)
    );
    
    if (shared == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }
    
    // 创建信号量
    sem_t* sem_producer = sem_open(SEM_PRODUCER, O_CREAT, 0666, 1);
    sem_t* sem_consumer = sem_open(SEM_CONSUMER, O_CREAT, 0666, 0);
    
    if (sem_producer == SEM_FAILED || sem_consumer == SEM_FAILED) {
        perror("sem_open failed");
        return 1;
    }
    
    // 初始化
    shared->running = true;
    shared->write_pos = 0;
    shared->read_pos = 0;
    shared->total_messages = 0;
    shared->total_bytes = 0;
    shared->start_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    
    std::cout << "[Producer] Sending " << NUM_MESSAGES << " messages..." << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_MESSAGES && g_running; ++i) {
        sem_wait(sem_producer);
        
        if (!shared->running) break;
        
        // 写入消息
        size_t pos = shared->write_pos.load() % (SHM_SIZE / MSG_SIZE);
        char* msg_ptr = shared->buffer + (pos * MSG_SIZE);
        
        snprintf(msg_ptr, MSG_SIZE, "Message #%zu from producer", i);
        
        shared->write_pos.fetch_add(1);
        shared->total_messages.fetch_add(1);
        shared->total_bytes.fetch_add(MSG_SIZE);
        
        sem_post(sem_consumer);
        
        if (i % 10000 == 0) {
            std::cout << "[Producer] Sent " << i << " messages" << std::endl;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    double throughput = (shared->total_messages.load() * 1000.0) / duration;
    double bandwidth = (shared->total_bytes.load() / 1024.0 / 1024.0) * 1000.0 / duration;
    
    std::cout << "\n=== Producer Results ===" << std::endl;
    std::cout << "Total messages: " << shared->total_messages.load() << std::endl;
    std::cout << "Total bytes: " << shared->total_bytes.load() << std::endl;
    std::cout << "Duration: " << duration << " ms" << std::endl;
    std::cout << "Throughput: " << throughput << " msg/s" << std::endl;
    std::cout << "Bandwidth: " << bandwidth << " MB/s" << std::endl;
    
    shared->running = false;
    sem_post(sem_consumer); // Wake up consumer
    
    munmap(shared, sizeof(SharedData));
    close(shm_fd);
    sem_close(sem_producer);
    sem_close(sem_consumer);
    
    return 0;
}

int run_consumer() {
    std::cout << "[Consumer] Starting..." << std::endl;
    
    // 等待共享内存创建
    sleep(1);
    
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        return 1;
    }
    
    SharedData* shared = reinterpret_cast<SharedData*>(
        mmap(nullptr, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)
    );
    
    if (shared == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }
    
    sem_t* sem_producer = sem_open(SEM_PRODUCER, 0666);
    sem_t* sem_consumer = sem_open(SEM_CONSUMER, 0666);
    
    if (sem_producer == SEM_FAILED || sem_consumer == SEM_FAILED) {
        perror("sem_open failed");
        return 1;
    }
    
    size_t received = 0;
    uint64_t total_latency = 0;
    
    std::cout << "[Consumer] Waiting for messages..." << std::endl;
    
    while (g_running && (shared->running.load() || received < shared->total_messages.load())) {
        sem_wait(sem_consumer);
        
        if (!shared->running.load() && received >= shared->total_messages.load()) {
            break;
        }
        
        // 读取消息
        size_t pos = shared->read_pos.load() % (SHM_SIZE / MSG_SIZE);
        char* msg_ptr = shared->buffer + (pos * MSG_SIZE);
        
        // 模拟处理延迟
        volatile char sum = 0;
        for (size_t i = 0; i < MSG_SIZE; ++i) {
            sum += msg_ptr[i];
        }
        (void)sum;
        
        shared->read_pos.fetch_add(1);
        received++;
        
        sem_post(sem_producer);
        
        if (received % 10000 == 0) {
            std::cout << "[Consumer] Received " << received << " messages" << std::endl;
        }
    }
    
    auto end_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    
    uint64_t duration_us = end_time - shared->start_time.load();
    double duration_ms = duration_us / 1000.0;
    double throughput = (received * 1000.0) / duration_ms;
    double bandwidth = (received * MSG_SIZE / 1024.0 / 1024.0) * 1000.0 / duration_ms;
    
    std::cout << "\n=== Consumer Results ===" << std::endl;
    std::cout << "Total received: " << received << std::endl;
    std::cout << "Duration: " << duration_ms << " ms" << std::endl;
    std::cout << "Throughput: " << throughput << " msg/s" << std::endl;
    std::cout << "Bandwidth: " << bandwidth << " MB/s" << std::endl;
    
    munmap(shared, sizeof(SharedData));
    close(shm_fd);
    sem_close(sem_producer);
    sem_close(sem_consumer);
    
    return 0;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <producer|consumer>" << std::endl;
        return 1;
    }
    
    std::string role = argv[1];
    
    if (role == "producer") {
        return run_producer();
    } else if (role == "consumer") {
        return run_consumer();
    } else {
        std::cerr << "Invalid role. Use 'producer' or 'consumer'" << std::endl;
        return 1;
    }
}
