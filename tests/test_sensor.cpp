#include <iostream>
#include <cassert>
#include <memory>
#include "lwipc/sensor_interface.hpp"

using namespace lwipc;

// 模拟传感器实现
class MockSensor : public ISensor {
private:
    SensorConfig config_;
    SensorStatus status_ = SensorStatus::INITIALIZING;
    bool running_ = false;
    uint64_t sequence_ = 0;
    
public:
    bool initialize(const SensorConfig& config) override {
        config_ = config;
        status_ = SensorStatus::OK;
        return true;
    }
    
    bool start() override {
        if (status_ != SensorStatus::OK) return false;
        running_ = true;
        status_ = SensorStatus::OK;
        return true;
    }
    
    void stop() override {
        running_ = false;
        status_ = SensorStatus::STOPPED;
    }
    
    std::shared_ptr<SensorData> getData(std::chrono::milliseconds timeout) override {
        if (!running_) return nullptr;
        
        auto data = std::make_shared<ImuData>();
        data->timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        data->sequence = sequence_++;
        data->status = status_;
        data->frame_id = config_.name;
        data->topic = config_.topic;
        
        // 模拟数据
        data->ax = 1.0;
        data->ay = 2.0;
        data->az = 9.8;
        
        return data;
    }
    
    SensorStatus getStatus() const override {
        return status_;
    }
    
    const SensorConfig& getConfig() const override {
        return config_;
    }
    
    bool selfTest() override {
        return true;
    }
};

void test_sensor_types() {
    std::cout << "Testing sensor data types... ";
    
    // 测试IMU数据
    ImuData imu;
    imu.ax = 9.8;
    imu.gx = 0.1;
    assert(imu.type == SensorType::IMU);
    
    // 测试激光雷达数据
    LidarData lidar;
    lidar.point_count = 1000;
    lidar.points.resize(1000);
    assert(lidar.type == SensorType::LIDAR);
    
    // 测试图像数据
    ImageData image;
    image.width = 1920;
    image.height = 1080;
    image.format = PixelFormat::RGB8;
    image.data.resize(1920 * 1080 * 3);
    assert(image.type == SensorType::CAMERA);
    
    // 测试GNSS数据
    GnssData gnss;
    gnss.latitude = 39.9042;
    gnss.longitude = 116.4074;
    gnss.fix_type = 3;  // 3D fix
    assert(gnss.type == SensorType::GNSS);
    
    // 测试距离数据
    DistanceData dist;
    dist.distance = 1.5f;
    dist.max_range = 5.0f;
    assert(dist.type == SensorType::ULTRASONIC);
    
    std::cout << "PASSED" << std::endl;
}

void test_mock_sensor() {
    std::cout << "Testing mock sensor... ";
    
    MockSensor sensor;
    
    // 配置
    SensorConfig config;
    config.name = "mock_imu";
    config.type = SensorType::IMU;
    config.topic = "/sensors/imu";
    config.sample_rate = 100;
    
    assert(sensor.initialize(config));
    assert(sensor.getStatus() == SensorStatus::OK);
    
    // 启动
    assert(sensor.start());
    
    // 获取数据
    auto data = sensor.getData(std::chrono::milliseconds(100));
    assert(data != nullptr);
    assert(data->type == SensorType::IMU);
    assert(data->status == SensorStatus::OK);
    assert(!data->topic.empty());
    
    // 停止
    sensor.stop();
    assert(sensor.getStatus() == SensorStatus::STOPPED);
    
    auto data_after_stop = sensor.getData(std::chrono::milliseconds(100));
    assert(data_after_stop == nullptr);
    
    std::cout << "PASSED" << std::endl;
}

void test_sensor_manager() {
    std::cout << "Testing sensor manager... ";
    
    SensorManager manager;
    
    // 初始为空
    assert(manager.getSensorCount() == 0);
    assert(manager.getSensorNames().empty());
    
    // 注册传感器
    auto sensor1 = std::make_shared<MockSensor>();
    auto sensor2 = std::make_shared<MockSensor>();
    
    assert(manager.registerSensor("imu", sensor1));
    assert(manager.registerSensor("camera", sensor2));
    
    assert(manager.getSensorCount() == 2);
    assert(manager.getSensorNames().size() == 2);
    
    // 获取传感器
    auto retrieved = manager.getSensor("imu");
    assert(retrieved != nullptr);
    assert(retrieved == sensor1);
    
    // 获取不存在的传感器
    auto not_found = manager.getSensor("nonexistent");
    assert(not_found == nullptr);
    
    // 注销传感器
    manager.unregisterSensor("camera");
    assert(manager.getSensorCount() == 1);
    assert(manager.getSensor("camera") == nullptr);
    
    std::cout << "PASSED" << std::endl;
}

void test_sensor_lifecycle() {
    std::cout << "Testing sensor lifecycle... ";
    
    SensorManager manager;
    
    auto sensor = std::make_shared<MockSensor>();
    manager.registerSensor("test_sensor", sensor);
    
    // 初始化但未启动
    SensorConfig config;
    config.name = "test";
    config.type = SensorType::IMU;
    sensor->initialize(config);
    
    // 启动所有
    assert(manager.startAll());
    assert(sensor->getStatus() == SensorStatus::OK);
    
    // 获取数据
    auto data = sensor->getData(std::chrono::milliseconds(100));
    assert(data != nullptr);
    
    // 停止所有
    manager.stopAll();
    assert(sensor->getStatus() == SensorStatus::STOPPED);
    
    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "=== Sensor Interface Tests ===" << std::endl;
    
    test_sensor_types();
    test_mock_sensor();
    test_sensor_manager();
    test_sensor_lifecycle();
    
    std::cout << "\nAll tests PASSED!" << std::endl;
    return 0;
}
