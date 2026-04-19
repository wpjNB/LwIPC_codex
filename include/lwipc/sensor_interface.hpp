#pragma once

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <chrono>
#include <optional>
#include <variant>
#include <any>
#include <unordered_map>
#include "message.hpp"

namespace lwipc {

/**
 * @brief 传感器数据类型枚举
 */
enum class SensorType {
    UNKNOWN = 0,
    LIDAR,        // 激光雷达
    CAMERA,       // 摄像头
    IMU,          // 惯性测量单元
    GNSS,         // 全球导航卫星系统
    ULTRASONIC,   // 超声波
    RADAR,        // 毫米波雷达
    TEMPERATURE,  // 温度传感器
    PRESSURE,     // 压力传感器
    CUSTOM        // 自定义传感器
};

/**
 * @brief 传感器数据状态
 */
enum class SensorStatus {
    OK = 0,
    INITIALIZING,
    ERROR,
    TIMEOUT,
    CALIBRATING,
    STOPPED
};

/**
 * @brief 通用传感器数据基类
 */
struct SensorData {
    uint64_t timestamp;      // 时间戳（纳秒）
    uint64_t sequence;       // 序列号
    SensorType type;         // 传感器类型
    SensorStatus status;     // 传感器状态
    std::string frame_id;    // 坐标系标识
    std::string topic;       // 关联的topic
    
    SensorData() 
        : timestamp(0)
        , sequence(0)
        , type(SensorType::UNKNOWN)
        , status(SensorStatus::OK)
        {}
    
    virtual ~SensorData() = default;
};

/**
 * @brief 激光雷达点云数据
 */
struct LidarPoint {
    float x, y, z;
    float intensity;
    uint16_t ring;
};

struct LidarData : public SensorData {
    uint32_t point_count;           // 点数
    std::vector<LidarPoint> points; // 点云数据
    float scan_time;                // 扫描时间（秒）
    
    LidarData() : point_count(0), scan_time(0.0f) {
        type = SensorType::LIDAR;
    }
};

/**
 * @brief 摄像头图像数据
 */
enum class PixelFormat {
    UNKNOWN = 0,
    RGB8,
    BGR8,
    RGBA8,
    BGRA8,
    MONO8,
    YUV422,
    COMPRESSED  // JPEG/PNG等压缩格式
};

struct ImageData : public SensorData {
    uint32_t width;
    uint32_t height;
    uint32_t step;        // 行字节数
    PixelFormat format;
    std::vector<uint8_t> data;  // 图像数据
    
    ImageData() 
        : width(0)
        , height(0)
        , step(0)
        , format(PixelFormat::UNKNOWN)
        {
            type = SensorType::CAMERA;
        }
};

/**
 * @brief IMU数据
 */
struct ImuData : public SensorData {
    // 加速度计 (m/s²)
    double ax, ay, az;
    // 陀螺仪 (rad/s)
    double gx, gy, gz;
    // 磁力计 (μT)
    double mx, my, mz;
    // 四元数姿态 (w, x, y, z)
    double qw, qx, qy, qz;
    // 温度 (°C)
    float temperature;
    
    ImuData() 
        : ax(0), ay(0), az(0)
        , gx(0), gy(0), gz(0)
        , mx(0), my(0), mz(0)
        , qw(1), qx(0), qy(0), qz(0)
        , temperature(0)
        {
            type = SensorType::IMU;
        }
};

/**
 * @brief GNSS数据
 */
struct GnssData : public SensorData {
    double latitude;   // 纬度（度）
    double longitude;  // 经度（度）
    double altitude;   // 海拔（米）
    double speed;      // 速度（m/s）
    double heading;    // 航向角（度）
    double pdop;       // 位置精度因子
    uint8_t fix_type;  // 定位类型：0=无，1=2D，2=3D，3=RTK
    uint8_t satellite_count;  // 可见卫星数
    
    GnssData()
        : latitude(0), longitude(0), altitude(0)
        , speed(0), heading(0), pdop(99.9)
        , fix_type(0), satellite_count(0)
        {
            type = SensorType::GNSS;
        }
};

/**
 * @brief 超声波/距离传感器数据
 */
struct DistanceData : public SensorData {
    float distance;  // 距离（米）
    float min_range; // 最小测量范围
    float max_range; // 最大测量范围
    
    DistanceData()
        : distance(0), min_range(0), max_range(0)
        {
            type = SensorType::ULTRASONIC;
        }
};

/**
 * @brief 传感器配置
 */
struct SensorConfig {
    std::string name;           // 传感器名称
    SensorType type;            // 传感器类型
    std::string device_path;    // 设备路径（如/dev/ttyUSB0）
    int sample_rate;            // 采样率（Hz）
    bool enabled;               // 是否启用
    QoSProfile qos;             // QoS配置
    std::string topic;          // 发布topic
    std::any extra_config;      // 额外配置（特定于传感器类型）
    
    SensorConfig()
        : type(SensorType::UNKNOWN)
        , sample_rate(0)
        , enabled(true)
        {}
};

/**
 * @brief 传感器接口抽象类
 */
class ISensor {
public:
    virtual ~ISensor() = default;
    
    // 初始化传感器
    virtual bool initialize(const SensorConfig& config) = 0;
    
    // 启动数据采集
    virtual bool start() = 0;
    
    // 停止数据采集
    virtual void stop() = 0;
    
    // 获取最新数据（阻塞/非阻塞）
    virtual std::shared_ptr<SensorData> getData(std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) = 0;
    
    // 获取传感器状态
    virtual SensorStatus getStatus() const = 0;
    
    // 获取传感器配置
    virtual const SensorConfig& getConfig() const = 0;
    
    // 校准传感器
    virtual bool calibrate() { return false; }
    
    // 自检
    virtual bool selfTest() { return true; }
};

/**
 * @brief 传感器管理器
 */
class SensorManager {
public:
    using SensorPtr = std::shared_ptr<ISensor>;
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SensorPtr> sensors_;
    
public:
    SensorManager() = default;
    ~SensorManager() = default;
    
    // 注册传感器
    bool registerSensor(const std::string& name, SensorPtr sensor);
    
    // 注销传感器
    void unregisterSensor(const std::string& name);
    
    // 获取传感器
    SensorPtr getSensor(const std::string& name) const;
    
    // 获取所有传感器名称
    std::vector<std::string> getSensorNames() const;
    
    // 启动所有传感器
    bool startAll();
    
    // 停止所有传感器
    void stopAll();
    
    // 获取传感器数量
    size_t getSensorCount() const;
};

} // namespace lwipc
