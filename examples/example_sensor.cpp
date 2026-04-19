/**
 * @file example_sensor.cpp
 * @brief 传感器接入示例
 * 
 * 演示如何使用 LwIPC 的传感器接口接入多种传感器
 */

#include <lwipc/sensor_interface.hpp>
#include <lwipc/broker.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <memory>

using namespace lwipc;

// 自定义激光雷达传感器实现
class SimulatedLiDAR : public ISensor {
public:
    SimulatedLiDAR() : initialized_(false), running_(false) {}
    
    bool initialize(const SensorConfig& config) override {
        std::cout << "[LiDAR] Initializing " << config.name << std::endl;
        config_ = config;
        initialized_ = true;
        return true;
    }
    
    bool start() override {
        if (!initialized_) return false;
        running_ = true;
        std::cout << "[LiDAR] Starting " << config_.name << std::endl;
        return true;
    }
    
    void stop() override {
        running_ = false;
        std::cout << "[LiDAR] Stopped" << std::endl;
    }
    
    std::shared_ptr<SensorData> getData(std::chrono::milliseconds timeout) override {
        static std::mt19937 gen(42);
        static std::uniform_real_distribution<> dist(0.1f, 10.0f);
        
        auto data = std::make_shared<LidarData>();
        data->timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        data->sequence = sequence_++;
        
        // 生成 360 个点的模拟数据
        data->points.resize(360);
        for (int i = 0; i < 360; ++i) {
            data->points[i].x = dist(gen);
            data->points[i].y = dist(gen);
            data->points[i].z = 0.0f;
            data->points[i].intensity = dist(gen);
        }
        
        return data;
    }
    
    SensorStatus getStatus() const override {
        if (!initialized_) return SensorStatus::INITIALIZING;
        if (running_) return SensorStatus::OK;
        return SensorStatus::STOPPED;
    }
    
    const SensorConfig& getConfig() const override {
        return config_;
    }
    
private:
    bool initialized_;
    std::atomic<bool> running_;
    uint64_t sequence_{0};
    SensorConfig config_;
};

// 自定义 IMU 传感器实现
class SimulatedIMU : public ISensor {
public:
    SimulatedIMU() : initialized_(false), running_(false) {}
    
    bool initialize(const SensorConfig& config) override {
        std::cout << "[IMU] Initializing " << config.name << std::endl;
        config_ = config;
        initialized_ = true;
        return true;
    }
    
    bool start() override {
        if (!initialized_) return false;
        running_ = true;
        std::cout << "[IMU] Starting " << config_.name << std::endl;
        return true;
    }
    
    void stop() override {
        running_ = false;
        std::cout << "[IMU] Stopped" << std::endl;
    }
    
    std::shared_ptr<SensorData> getData(std::chrono::milliseconds timeout) override {
        static std::mt19937 gen(123);
        static std::uniform_real_distribution<> dist(-1.0f, 1.0f);
        
        auto data = std::make_shared<ImuData>();
        data->timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        data->sequence = sequence_++;
        
        data->ax = dist(gen);
        data->ay = dist(gen);
        data->az = dist(gen) + 9.8f;  // 重力加速度
        data->gx = dist(gen) * 0.1f;
        data->gy = dist(gen) * 0.1f;
        data->gz = dist(gen) * 0.1f;
        data->qw = 1.0;
        data->qx = 0.0;
        data->qy = 0.0;
        data->qz = 0.0;
        
        return data;
    }
    
    SensorStatus getStatus() const override {
        if (!initialized_) return SensorStatus::INITIALIZING;
        if (running_) return SensorStatus::OK;
        return SensorStatus::STOPPED;
    }
    
    const SensorConfig& getConfig() const override {
        return config_;
    }
    
private:
    bool initialized_;
    std::atomic<bool> running_;
    uint64_t sequence_{0};
    SensorConfig config_;
};

int main() {
    SensorManager manager;
    
    // 创建传感器配置
    SensorConfig lidar_config;
    lidar_config.name = "main_lidar";
    lidar_config.type = SensorType::LIDAR;
    lidar_config.sample_rate = 10;
    
    SensorConfig imu_config;
    imu_config.name = "main_imu";
    imu_config.type = SensorType::IMU;
    imu_config.sample_rate = 100;
    
    // 创建传感器实例
    auto lidar = std::make_shared<SimulatedLiDAR>();
    auto imu = std::make_shared<SimulatedIMU>();
    
    // 注册传感器
    std::cout << "=== Registering Sensors ===" << std::endl;
    manager.registerSensor("main_lidar", std::dynamic_pointer_cast<ISensor>(lidar));
    manager.registerSensor("main_imu", std::dynamic_pointer_cast<ISensor>(imu));
    
    // 初始化传感器
    std::cout << "\n=== Initializing Sensors ===" << std::endl;
    lidar->initialize(lidar_config);
    imu->initialize(imu_config);
    
    // 启动所有传感器
    std::cout << "\n=== Starting Sensors ===" << std::endl;
    manager.startAll();
    
    // 读取数据
    std::cout << "\n=== Reading Sensor Data ===" << std::endl;
    
    for (int i = 0; i < 3; ++i) {
        // 读取 LiDAR 数据
        auto lidar_data = std::static_pointer_cast<LidarData>(lidar->getData(std::chrono::milliseconds(100)));
        if (lidar_data) {
            std::cout << "[LiDAR] Timestamp: " << lidar_data->timestamp 
                      << ", Points: " << lidar_data->points.size() << std::endl;
        }
        
        // 读取 IMU 数据
        auto imu_data = std::static_pointer_cast<ImuData>(imu->getData(std::chrono::milliseconds(100)));
        if (imu_data) {
            std::cout << "[IMU] Timestamp: " << imu_data->timestamp 
                      << ", Accel: (" << imu_data->ax << ", " 
                      << imu_data->ay << ", " << imu_data->az << ")" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 停止传感器
    std::cout << "\n=== Stopping Sensors ===" << std::endl;
    manager.stopAll();
    
    // 获取传感器名称列表
    std::cout << "\n=== Sensor List ===" << std::endl;
    auto names = manager.getSensorNames();
    for (const auto& name : names) {
        std::cout << "Sensor: " << name << std::endl;
    }
    
    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}
