#ifndef ETHERNET_MODULE_HPP
#define ETHERNET_MODULE_HPP

#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>

#include "button-led.hpp"

class SmartSocket {
private:
    std::unique_ptr<int, std::function<void(int*)>> socket_fd_;
    
public:
    SmartSocket();
    explicit SmartSocket(int fd);
    ~SmartSocket() = default;
    
    bool create();
    int get() const;
    void reset(int fd);
    bool isValid() const;
    void close();
    
    SmartSocket(const SmartSocket&) = delete;
    SmartSocket& operator=(const SmartSocket&) = delete;
    
    SmartSocket(SmartSocket&&) = default;
    SmartSocket& operator=(SmartSocket&&) = default;
};

class SmartClient {
private:
    std::unique_ptr<SmartSocket> socket_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread sender_thread_;
    std::thread receiver_thread_;
    std::atomic<int> message_counter_{0};

    std::queue<std::vector<uint8_t>> send_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    void sendingLoop();
    void receivingLoop();
    bool checkConnection();
    void cleanup();
    
    SysfsLedController* led1_ = nullptr;
    SysfsLedController* led2_ = nullptr;
    
    bool sendDataInternal(const std::vector<uint8_t>& data);
    
public:
    SmartClient();
    ~SmartClient();
    
    // Основные методы управления
    bool start(const std::string& ip, int port);
    void stop();
    
    // Проверка состояния
    bool isRunning() const;
    bool isConnected() const;
    bool checkEthernetLink();
    
    // Получить статистику
    int getMessageCount() const;

    bool sendData(const std::vector<uint8_t>& data);

    void setupLed(SysfsLedController* led1, SysfsLedController* led2);
    
    // Удаляем копирование
    SmartClient(const SmartClient&) = delete;
    SmartClient& operator=(const SmartClient&) = delete;
};

#endif