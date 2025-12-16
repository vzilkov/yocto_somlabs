#ifndef BUTTON_LED_HPP
#define BUTTON_LED_HPP

#include <atomic>
#include <string>
#include <thread>

class SysfsLedController {
private:
    std::string ledPath;
    std::string ledName;
    std::atomic<bool> blinking{false};
    std::thread blinkThread;

    char brightness_ = 0;
        
public:
    explicit SysfsLedController(const std::string& led_name);
    
    ~SysfsLedController();
    
    SysfsLedController(const SysfsLedController&) = delete;
    SysfsLedController& operator=(const SysfsLedController&) = delete;
    
    SysfsLedController(SysfsLedController&&) noexcept = default;
    SysfsLedController& operator=(SysfsLedController&&) noexcept = default;
    
    bool led_set(const char val);
    bool turnOn();
    bool turnOff();
    bool toggle();
};


#endif