#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <csignal>
#include <gpiod.hpp> // ver 2.2.1

#include "button-led.hpp"
#include "ethernet.hpp"

// // Конфигурация
constexpr int LED_GPIO = 12;
constexpr int BUTTON_GPIO = 8;   
constexpr int BUTTON_CHIP = 0;

constexpr int port_num = 8080;
const std::string ip_adr = "192.168.31.27";

void SysfsLedController::internal_thread(){
    while(running){
        const int sec = 1000; //ms
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        const unsigned int bright_low = 0;
        const unsigned int bright_high = 0xFF;
        switch(statement_){
            case BLINK:
                if(blinking){
                    led_set(bright_high);
                    std::this_thread::sleep_for(std::chrono::milliseconds(sec/freq_hz_atomic));
                    led_set(bright_low);
                    blinking = false;
                }
                break;
            case ON:
                led_set(bright_high);
                break;
            case OFF:
                led_set(bright_low);
                break;
        };
    }
}

void SysfsLedController::blink(int hz){
    statement_ = BLINK;
    blinking = true;
    if(hz <= 0) hz=1;
    freq_hz_atomic = hz;
}

void SysfsLedController::switchON(){
    statement_ = ON;
}

void SysfsLedController::switchOFF(){
    statement_ = OFF;
}

SysfsLedController::~SysfsLedController() {
    running = false;
    if(ledThread_.joinable())
        ledThread_.join();
    switchOFF();
}

bool SysfsLedController::led_set(const char val){
    try {
        auto brightnessFile = std::make_unique<std::ofstream>(ledPath + "/brightness");
        if (!brightnessFile->is_open()) {
            return false;
        }
        brightness_ = val;
        *brightnessFile << static_cast<int>(brightness_);
        // std::cout << ledName << " sent value " << brightness_ << std::endl;
        return true;
    }
    catch(...){
        return false;
    }
}

SysfsLedController::SysfsLedController(const std::string& led_name) : ledName(led_name) {
    ledPath = "/sys/class/leds/" + ledName;
        
    std::cout << "LED '" << ledName << "' initialized" << std::endl;
    ledThread_ = std::thread(&SysfsLedController::internal_thread, this);
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

std::atomic<bool> program_running{true};

void signalHandler(int signal) {
    std::cout << "\n[MAIN] Received signal " << signal << ", shutting down..." << std::endl;
    program_running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);

    try {
        SysfsLedController led2("LED-IO-12");
        SysfsLedController led1("LED-IO-11");
        
        const int pin_num = 8;
        SimpleButton gpio08(pin_num, true);

        SmartClient client;
        client.setupLed(&led1, &led2);

        std::string statement = "normal";  
        int connection_attempts = 0;  
        const int MAX_ATTEMPTS = 5;
        
        led2.switchOFF();
        led1.blink(2);

        while(program_running){
            static bool eth_link_was_up = true;
            bool eth_link_is_up = client.checkEthernetLink();

            if(eth_link_is_up == false && eth_link_was_up == true){
                std::cerr << "[MAIN] ETH0 LINK DOWN - Cable disconnected!" << std::endl;
                statement = "alert";
                if (client.isRunning()) {
                    client.stop();
                }
                led1.switchON();
            }
            else if (eth_link_was_up == false && eth_link_is_up == true) {
                // Кабель подключен
                std::cout << "[MAIN] ETH0 LINK UP - Cable connected" << std::endl;
            }

            eth_link_was_up = eth_link_is_up;

            if(statement == "normal")
            {
                
                if (!client.isRunning())
                {
                    std::cout << "Starting client..." << std::endl;// всё время заходит сюда до проверки соединения не доходит
                    if (client.start(ip_adr, port_num)) {
                        std::cout << "Client started successfully" << std::endl;
                        connection_attempts = 0;
                        led2.switchOFF();
                        led1.switchON();
                    } else {
                        std::cout << "Failed to start client" << std::endl;
                        connection_attempts++;

                        
                    }
                }

                if(!client.isConnected()){
                    std::cerr << "[MAIN] Ethernet connection lost!" << std::endl;
                    client.stop();
                    connection_attempts++;
                    if (connection_attempts >= MAX_ATTEMPTS) {
                        std::cerr << "[MAIN] Too many failed attempts, switching to ALERT" << std::endl;
                        statement = "alert";
                        continue;
                    }
                    led1.blink(2); // rk_func_boot_connection_stop
                    led2.switchOFF();
                }
                else{
                    if(connection_attempts > 0){
                        static auto last_reset_time = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_reset_time).count() > 10) {
                            std::cout << "[MAIN] Connection stable for 10s, resetting attempt counter" << std::endl;
                            connection_attempts = 0;
                            last_reset_time = now;
                        }
                    }
                    static auto last_send_time = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    
                    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_send_time).count() >= 1) { // 1 seconds period to send
                        std::vector<uint8_t> buf = {0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39};// imitation from sensor
                        client.sendData(buf);
                        last_send_time = now;
                    }
                }
            }
            else if(statement == "alert")
            {
                led1.blink(2); // rk_func_boot_connection_stop
                led2.switchOFF();
                std::cout << "[ETHERNET] Catched ETH disconnection" << std::endl;
                if (client.isRunning()) {
                    std::cout << "[MAIN] Stopping client in ALERT mode" << std::endl;
                    client.stop();
                }
            }// alert end if

            if(gpio08.isPressed()) { // rk_func_sensor_alert_reaction
                std::cout << "[MAIN] Button pressed" << std::endl;
                if(statement == "alert") {
                    std::cout << "[MAIN] switching to NORMAL" << std::endl;
                    statement = "normal";
                    connection_attempts = 0;
                }
            }

            std::cout   << "[STATUS] State: " << statement 
                        << ", ETH running: " << client.isRunning()
                        << ", ETH connected: " << client.isConnected() 
                        << ", Attempts: " << connection_attempts << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

    //[STATUS] State: normal, ETH running: 0, ETH connected: 0, Attempts: 32 // not working
    // [STATUS] State: normal, ETH running: 1, ETH connected: 1, Attempts: 0 // working well
    // идея сделать этот поток как управляющий
    // тут машина состояний

    } catch (const std::exception& e) {
        std::cerr << "*** Critical error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "*** Program ended" << std::endl;
    return 0;
}
