#ifndef BUTTON_LED_HPP
#define BUTTON_LED_HPP

#include <atomic>
#include <string>
#include <thread>

#define STANDARD_LED_FREQ_BLINK_HZ 2

enum led_statement_e{
        BLINK, ON, OFF
    };

class SysfsLedController {
private:
    std::string ledPath;
    std::string ledName;
    std::atomic<bool> blinking{true};
    std::atomic<bool> running{true};
    std::thread ledThread_;
    std::atomic<unsigned int> freq_hz_atomic{STANDARD_LED_FREQ_BLINK_HZ};

    char brightness_ = 0;
    
    std::atomic<led_statement_e> statement_{BLINK};
    
    void internal_thread();
        
public:
    explicit SysfsLedController(const std::string& led_name);
    
    ~SysfsLedController();
    
    SysfsLedController(const SysfsLedController&) = delete;
    SysfsLedController& operator=(const SysfsLedController&) = delete;
    
    SysfsLedController(SysfsLedController&&) noexcept = default;
    SysfsLedController& operator=(SysfsLedController&&) noexcept = default;
    
    void ledLoop();

    bool led_set(const char val);
    void switchON();
    void switchOFF();
    void blink(int hz);
    void stop_thread(bool isContinue);
};

#endif