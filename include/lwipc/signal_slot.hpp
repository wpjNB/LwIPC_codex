#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <any>
#include "executor.hpp"

namespace lwipc {

/**
 * @brief 信号槽机制 - 观察者模式实现
 * 
 * 支持：
 * - 多对多连接（多个信号连接多个槽）
 * - 同步/异步回调
 * - 线程安全
 * - 自动断开连接
 */

// 连接句柄，用于断开特定连接
struct Connection {
    uint64_t id;
    bool valid;
    std::weak_ptr<void> slot_ref;  // 引用槽对象
    
    Connection() : id(0), valid(false) {}
    Connection(uint64_t id_) : id(id_), valid(true) {}
    Connection(uint64_t id_, std::shared_ptr<void> slot) : id(id_), valid(true), slot_ref(slot) {}
    
    void disconnect() { 
        if (valid) {
            valid = false;
            // 尝试通过 slot_ref 获取槽并标记为无效
            if (auto slot = slot_ref.lock()) {
                // 槽仍然存在，但我们在 Signal::disconnect 中处理实际的无效标记
            }
        }
    }
    bool isConnected() const { 
        if (!valid) return false;
        // 检查槽是否仍然有效
        if (auto slot = slot_ref.lock()) {
            return true;
        }
        return false;
    }
};

// 信号基类（类型擦除）
class SignalBase {
public:
    virtual ~SignalBase() = default;
    virtual void disconnect(Connection conn) = 0;
    virtual size_t getConnectionCount() const = 0;
};

// 信号实现模板
template<typename... Args>
class Signal : public SignalBase {
public:
    using CallbackType = std::function<void(Args...)>;
    
private:
    struct SlotEntry {
        uint64_t id;
        CallbackType callback;
        std::atomic<bool> valid{true};
    };
    
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<SlotEntry>> slots_;
    std::atomic<uint64_t> next_id_{1};
    std::atomic<size_t> active_count_{0};
    
public:
    Signal() = default;
    ~Signal() override = default;
    
    // 连接槽函数（同步回调）
    Connection connect(CallbackType callback) {
        uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);
        auto entry = std::make_shared<SlotEntry>();
        entry->id = id;
        entry->callback = std::move(callback);
        
        std::lock_guard<std::mutex> lock(mutex_);
        slots_.push_back(entry);
        active_count_.fetch_add(1, std::memory_order_relaxed);
        
        return Connection(id, entry);
    }
    
    // 简化版connect（使用lambda或std::function）
    Connection connectSimple(std::function<void(Args...)> callback) {
        return connect(std::move(callback));
    }
    
    // 断开连接
    void disconnect(Connection conn) {
        if (!conn.valid) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& slot : slots_) {
            if (slot->id == conn.id) {
                if (slot->valid.exchange(false)) {
                    active_count_.fetch_sub(1, std::memory_order_relaxed);
                }
                return;  // 找到后立即返回
            }
        }
    }
    
    // 断开连接（通过 Connection 对象方法）
    void disconnectConnection(Connection& conn) {
        if (!conn.valid) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& slot : slots_) {
            if (slot->id == conn.id) {
                if (slot->valid.exchange(false)) {
                    active_count_.fetch_sub(1, std::memory_order_relaxed);
                }
                conn.valid = false;  // 更新传入的 Connection 对象
                return;
            }
        }
    }
    
    // 重载 disconnect 以支持通过 ID 断开（基类接口）
    void disconnect(Connection conn) override {
        if (!conn.valid) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& slot : slots_) {
            if (slot->id == conn.id) {
                if (slot->valid.exchange(false)) {
                    active_count_.fetch_sub(1, std::memory_order_relaxed);
                }
                return;
            }
        }
    }
    
    // 断开所有连接
    void disconnectAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& slot : slots_) {
            if (slot->valid.exchange(false)) {
                active_count_.fetch_sub(1, std::memory_order_relaxed);
            }
        }
        slots_.clear();
    }
    
    // 发射信号（同步调用所有槽）
    void emit(Args... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& slot : slots_) {
            if (slot->valid.load(std::memory_order_acquire)) {
                try {
                    slot->callback(args...);
                } catch (...) {
                    // 捕获异常，继续调用其他槽
                }
            }
        }
    }
    
    // 便捷操作符
    void operator()(Args... args) {
        emit(std::forward<Args>(args)...);
    }
    
    size_t getConnectionCount() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& slot : slots_) {
            if (slot->valid.load(std::memory_order_relaxed)) {
                count++;
            }
        }
        return count;
    }
    
    // 清理已断开的槽（可选调用）
    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        slots_.erase(
            std::remove_if(slots_.begin(), slots_.end(),
                [](const std::shared_ptr<SlotEntry>& slot) {
                    return !slot->valid.load(std::memory_order_relaxed);
                }),
            slots_.end()
        );
    }
};

// 信号槽管理器
class SignalManager {
public:
    using SignalPtr = std::shared_ptr<SignalBase>;
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SignalPtr> signals_;
    
public:
    SignalManager() = default;
    ~SignalManager() = default;
    
    // 获取或创建信号
    template<typename... Args>
    std::shared_ptr<Signal<Args...>> getOrCreate(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = signals_.find(name);
        if (it != signals_.end()) {
            return std::static_pointer_cast<Signal<Args...>>(it->second);
        }
        
        auto signal = std::make_shared<Signal<Args...>>();
        signals_[name] = signal;
        return signal;
    }
    
    // 检查信号是否存在
    bool hasSignal(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return signals_.find(name) != signals_.end();
    }
    
    // 移除信号
    void removeSignal(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        signals_.erase(name);
    }
    
    // 获取所有信号名称
    std::vector<std::string> getSignalNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(signals_.size());
        for (const auto& [name, _] : signals_) {
            names.push_back(name);
        }
        return names;
    }
    
    size_t getSignalCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return signals_.size();
    }
};

} // namespace lwipc
