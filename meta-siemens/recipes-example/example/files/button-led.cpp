#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <string>
#include <gpiod.hpp> // ver 2.2.1

// // Конфигурация
constexpr int LED_GPIO = 12;      // GPIO для светодиода (через sysfs)
constexpr int BUTTON_GPIO = 8;   // GPIO для кнопки (через libgpiod)
constexpr int BUTTON_CHIP = 0;   // Номер чипа gpio

class SysfsLedController {
private:
    std::string ledPath;
    std::string ledName;
    std::atomic<bool> blinking{false};
    std::thread blinkThread;
    
public:
    // Конструктор с именем LED (например "LED-IO-12")
    SysfsLedController(const std::string& led_name) : ledName(led_name) {
        ledPath = "/sys/class/leds/" + ledName;
        
        // if (!isAvailable()) {
        //     std::cerr << "Error: LED '" << ledName << "' not available!" << std::endl;
        //     throw std::runtime_error("LED not available");
        // }
        
        std::cout << "LED '" << ledName << "' initialized" << std::endl;
    }

    bool led_set(char val){
        try {
            std::ofstream brightnessFile(ledPath + "/brightness");
            brightnessFile << std::to_string(val);
            brightnessFile.close();
            return true;
        }
        catch(...){
            return false;
        }
    }
    
    ~SysfsLedController() {
        // stopBlinking();
        // turnOff();
    }
};

// class ButtonLibgpiod {
// private:
//     bool initialized_;
//     int line_number_;
//     int chip_number_;
//     int poll_interval_ms_;
//     gpiod::chip chip_;
//     gpiod::line_request request_;

// public:
//     ButtonLibgpiod(const int chip, const int line, const int poll_interval_ms = 50) 
//     :   initialized_(false), 
//         line_number_(line), 
//         chip_number_(chip),
//         poll_interval_ms_(poll_interval_ms),
//         chip_(std::string("dev/gpiochip") + std::to_string(chip))
//     {
        
//             try {
//                 auto builder = chip_.prepare_request();
//                 gpiod::request_config req_config;
//                 req_config.set_consumer("button-monitor");

//                 // gpiod::line_settings settings;
//                 // settings.set_direction(gpiod::line_settings::DIRECTION_INPUT);

//                 gpiod::line_config line_config;
//                 // line_config.add_line_settings(line_number_, settings);

//                 // builder.set_request_config(req_config);
//                 // builder.set_line_config(line_config);

//                 request_ = builder.do_request();

                
//                 initialized_ = true;
//                 std::cout << "Button initialized: GPIO" << line_number_ 
//                         << " on chip " << chip_number_ << std::endl;
                
//             } catch (const std::exception& e) {
//                 std::cerr << "Button init error: " << e.what() << std::endl;
//                 throw;
//             }
//     }
    
//     bool isPressed() const {
//         return 0;//line_.get_value() == 0;
//     }
    
//     ~ButtonLibgpiod() {
//         if (initialized_) {
//             // line_.release();
//         }
//     }
// };

class SimpleButton {
private:
    std::string gpio_path_;
    int gpio_number_;
    bool active_low_;

public:
    SimpleButton(int gpio_number, bool active_low = true)
        : gpio_number_(gpio_number), active_low_(active_low) {
        
        // Экспортируем GPIO
        std::ofstream export_file("/sys/class/gpio/export");
        if (export_file.is_open()) {
            export_file << gpio_number_;
            export_file.close();
        }
        
        // Даем время на создание файлов
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Устанавливаем направление (вход)
        std::string direction_path = "/sys/class/gpio/gpio" + std::to_string(gpio_number_) + "/direction";
        std::ofstream direction_file(direction_path);
        if (direction_file.is_open()) {
            direction_file << "in";
            direction_file.close();
        }
        
        // Путь к файлу значения
        gpio_path_ = "/sys/class/gpio/gpio" + std::to_string(gpio_number_) + "/value";
        
        std::cout << "Button on GPIO" << gpio_number_ << " initialized" << std::endl;
    }
    
    bool isPressed() const {
        std::ifstream value_file(gpio_path_);
        if (!value_file.is_open()) {
            return false;
        }
        
        char value;
        value_file >> value;
        value_file.close();
        
        // '0' = LOW, '1' = HIGH (ASCII)
        if (active_low_) {
            return value == '0';  // Active-low: нажата когда LOW
        } else {
            return value == '1';  // Active-high: нажата когда HIGH
        }
    }
    
    ~SimpleButton() {
        std::ofstream unexport_file("/sys/class/gpio/unexport");
        if (unexport_file.is_open()) {
            unexport_file << gpio_number_;
            unexport_file.close();
        }
    }
};

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "Button-LED Control Program" << std::endl;
    std::cout << "Built for VisionCB-6ULL" << std::endl;
    std::cout << "LED: GPIO" << LED_GPIO << " (sysfs)" << std::endl;
    std::cout << "Button: GPIO" << BUTTON_GPIO << " (libgpiod)" << std::endl;
    std::cout << "========================================" << std::endl;
        
    try {
       SysfsLedController led12("LED-IO-12");
       SysfsLedController led11("LED-IO-11");
    //    ButtonLibgpiod gpio08(0,8,50);
        SimpleButton gpio08(8, true);
        while(1){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            led12.led_set(255);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            led12.led_set(0);

            if(gpio08.isPressed()){
                led11.led_set(255);
            }
            else{
                led11.led_set(0);
            }
       }

    } catch (const std::exception& e) {
        std::cerr << "*** Critical error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "*** Program ended" << std::endl;
    return 0;
}
