#include <iostream>
#include <fstream>
#include <chrono>
#include <csignal>
#include <gpiod.hpp> // ver 2.2.1

#include "button-led.hpp"

// // Конфигурация
constexpr int LED_GPIO = 12;
constexpr int BUTTON_GPIO = 8;   
constexpr int BUTTON_CHIP = 0;

SysfsLedController::SysfsLedController(const std::string& led_name) : ledName(led_name) {
    ledPath = "/sys/class/leds/" + ledName;
        
    std::cout << "LED '" << ledName << "' initialized" << std::endl;
}

bool SysfsLedController::led_set(const char val){
    try {
        auto brightnessFile = std::make_unique<std::ofstream>(ledPath + "/brightness");
        if (!brightnessFile->is_open()) {
            return false;
        }
        brightness_ = val;
        *brightnessFile << static_cast<int>(brightness_);
        std::cout << ledName << " sent value " << brightness_ << std::endl;
        return true;
    }
    catch(...){
        return false;
    }
}

bool SysfsLedController::toggle(){
        if(brightness_){
            brightness_ = 0;
        }
        else{
            brightness_ = 0xFF;
        }
    return led_set(brightness_);
}

bool SysfsLedController::switchON(){
    brightness_ = 0xFF;
    return led_set(brightness_);
}

bool SysfsLedController::switchOFF(){
    brightness_ = 0;
    return led_set(brightness_);
}

SysfsLedController::~SysfsLedController() {
    switchOFF();
    // stopBlinking();
    // turnOff(); // Гарантированно выключаем при выходе
}


class SimpleButton {
private:
    std::string gpio_path_;
    int gpio_number_;
    bool active_low_;

public:
    SimpleButton(int gpio_number, bool active_low = true)
        : gpio_number_(gpio_number), active_low_(active_low) {
        
        std::ofstream export_file("/sys/class/gpio/export");
        if (export_file.is_open()) {
            export_file << gpio_number_;
            export_file.close();
        }
        
        // Даем время на создание файлов
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        
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
        
        if (active_low_) {
            return value == '0';  
        } else {
            return value == '1';
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
            if(led12.toggle()){
                std::cout<<"Led12 toggles" << std::endl;
            }
            else{
                std::cout<<"Led12 error" << std::endl;
            }

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
