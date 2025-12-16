#ifndef ETHERNET_MODULE_HPP
#define ETHERNET_MODULE_HPP

#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

class SmartSocket {
private:
    std::unique_ptr<int, std::function<void(int*)>> socket_fd_;
    
    static void socket_deleter(int* fd);
    
public:
    SmartSocket();    
    explicit SmartSocket(int fd);
    bool create();    
    int get() const;    
    void reset(int fd);    
    bool isValid() const;
};

class SmartClient {
private:
    SmartSocket client_socket_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread receive_thread_;
    
    void receive_impl();

public:
    SmartClient();
    
    bool connectToServer(const std::string& server_ip, int port);
    bool sendData(const std::string& data);
    void startReceiving();
    void disconnect();

    bool isConnected() const;
    bool isRunning() const;
    
    ~SmartClient();
};

#endif