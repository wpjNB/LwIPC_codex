#include "lwipc/sensor_interface.hpp"
#include <algorithm>

namespace lwipc {

bool SensorManager::registerSensor(const std::string& name, SensorPtr sensor) {
    if (!sensor) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto result = sensors_.emplace(name, std::move(sensor));
    return result.second;  // true if inserted, false if already exists
}

void SensorManager::unregisterSensor(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sensors_.find(name);
    if (it != sensors_.end()) {
        it->second->stop();
        sensors_.erase(it);
    }
}

SensorManager::SensorPtr SensorManager::getSensor(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sensors_.find(name);
    if (it != sensors_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> SensorManager::getSensorNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(sensors_.size());
    for (const auto& [name, _] : sensors_) {
        names.push_back(name);
    }
    return names;
}

bool SensorManager::startAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    bool all_success = true;
    
    for (auto& [name, sensor] : sensors_) {
        if (!sensor->start()) {
            all_success = false;
        }
    }
    
    return all_success;
}

void SensorManager::stopAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, sensor] : sensors_) {
        sensor->stop();
    }
}

size_t SensorManager::getSensorCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sensors_.size();
}

} // namespace lwipc
