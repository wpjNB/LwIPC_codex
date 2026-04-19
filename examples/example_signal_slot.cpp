/**
 * @file example_signal_slot.cpp
 * @brief 信号槽机制示例
 * 
 * 演示如何使用 LwIPC 的信号槽功能实现事件驱动编程
 */

#include <lwipc/signal_slot.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

using namespace lwipc;

// 定义信号类型
struct ButtonClickSignal {
    int button_id;
    std::string label;
};

struct DataUpdateSignal {
    double value;
    uint64_t timestamp;
};

class EventHandler {
public:
    void on_button_click(const ButtonClickSignal& event) {
        std::cout << "[Slot] Button clicked: id=" << event.button_id 
                  << ", label=" << event.label << std::endl;
    }
    
    void on_data_update(const DataUpdateSignal& event) {
        std::cout << "[Slot] Data updated: value=" << event.value 
                  << ", timestamp=" << event.timestamp << std::endl;
    }
    
    static void static_handler(const ButtonClickSignal& event) {
        std::cout << "[Static Slot] Button event: " << event.button_id << std::endl;
    }
};

int main() {
    SignalHub hub;
    EventHandler handler;
    
    // 创建信号
    auto button_signal = hub.create_signal<ButtonClickSignal>("ui/button_click");
    auto data_signal = hub.create_signal<DataUpdateSignal>("sensor/data_update");
    
    // 绑定槽函数（成员函数）
    hub.connect("ui/button_click", &EventHandler::on_button_click, &handler);
    
    // 绑定槽函数（静态函数）
    hub.connect("ui/button_click", &EventHandler::static_handler);
    
    // 绑定 Lambda 表达式
    hub.connect("sensor/data_update", [](const DataUpdateSignal& event) {
        std::cout << "[Lambda Slot] Received data: " << event.value << std::endl;
    });
    
    // 绑定另一个成员函数
    hub.connect("sensor/data_update", &EventHandler::on_data_update, &handler);
    
    std::cout << "=== Emitting Button Click Signals ===" << std::endl;
    
    // 发射信号
    for (int i = 1; i <= 3; ++i) {
        ButtonClickSignal click_event{i, "Button_" + std::to_string(i)};
        button_signal->emit(click_event);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    std::cout << "\n=== Emitting Data Update Signals ===" << std::endl;
    
    for (int i = 0; i < 3; ++i) {
        DataUpdateSignal update_event{
            static_cast<double>(i) * 10.5,
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()
        };
        data_signal->emit(update_event);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // 断开连接示例
    std::cout << "\n=== Disconnecting one slot ===" << std::endl;
    hub.disconnect("ui/button_click", &EventHandler::static_handler);
    
    ButtonClickSignal final_click{99, "Final_Button"};
    button_signal->emit(final_click);
    
    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}
