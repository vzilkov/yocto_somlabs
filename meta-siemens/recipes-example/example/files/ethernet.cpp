#include <iostream>
#include <memory>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ethernet.hpp"

void SmartSocket::socket_deleter(int* fd) {
    if (fd && *fd >= 0) {
        close(*fd);
        std::cout << "Socket closed: " << *fd << std::endl;
    }
    delete fd;
}

SmartSocket::SmartSocket() : 
    socket_fd_(nullptr, socket_deleter) {
    socket_fd_.reset(new int(-1));
}
    
SmartSocket::SmartSocket(int fd) : 
    socket_fd_(nullptr, socket_deleter) {
    socket_fd_.reset(new int(fd));
}
    
bool SmartSocket::create() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    
    socket_fd_.reset(new int(fd));
    return true;
}
    
int SmartSocket::get() const {
    return socket_fd_ ? *socket_fd_ : -1;
}
    
void SmartSocket::reset(int fd) {
    socket_fd_.reset(new int(fd));
}
    
bool SmartSocket::isValid() const {
    return socket_fd_ && *socket_fd_ >= 0;
}

SmartClient::SmartClient() = default;

bool SmartClient::connectToServer(const std::string& server_ip, int port) {
    // Создаем сокет
    if (!client_socket_.create()) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address or address not supported" << std::endl;
        return false;
    }
    
    // Устанавливаем таймаут на подключение
    struct timeval tv;
    tv.tv_sec = 5;  // 5 секунд таймаут
    tv.tv_usec = 0;
    setsockopt(client_socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // Подключаемся к серверу
    if (connect(client_socket_.get(), (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        return false;
    }
    
    connected_.store(true);
    std::cout << "Connected to server " << server_ip << ":" << port << std::endl;
    
    return true;
}

bool SmartClient::sendData(const std::string& data) {
    if (!connected_.load() || !client_socket_.isValid()) {
        std::cerr << "Not connected to server" << std::endl;
        return false;
    }
    
    ssize_t bytes_sent = send(client_socket_.get(), data.c_str(), data.length(), 0);
    
    if (bytes_sent < 0) {
        std::cerr << "Failed to send data" << std::endl;
        return false;
    }
    
    std::cout << "Sent " << bytes_sent << " bytes to server" << std::endl;
    return true;
}

void SmartClient::receive_impl() {
    char buffer[1024];
    
    while (running_.load() && connected_.load()) {
        memset(buffer, 0, sizeof(buffer));
        
        // Читаем данные от сервера
        ssize_t bytes_received = recv(client_socket_.get(), buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received > 0) {
            std::cout << "Received from server: " << buffer << std::endl;
            
            // Здесь можно добавить обработку полученных данных
            // Например, вызов callback-функции
            
        } else if (bytes_received == 0) {
            std::cout << "Server disconnected" << std::endl;
            connected_.store(false);
            running_.store(false);
            break;
        } else {
            // Таймаут или ошибка
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Это таймаут, просто продолжаем
                continue;
            } else {
                std::cerr << "Receive error" << std::endl;
                connected_.store(false);
                running_.store(false);
                break;
            }
        }
    }
    
    std::cout << "Receive thread exiting" << std::endl;
}

void SmartClient::startReceiving() {
    if (!connected_.load()) {
        std::cerr << "Not connected to server" << std::endl;
        return;
    }
    
    if (running_.load()) {
        std::cout << "Receive thread already running" << std::endl;
        return;
    }
    
    running_.store(true);
    
    // Создаем поток для приема данных
    receive_thread_ = std::thread([this]() {
        this->receive_impl();
    });
    
    // Detach поток - он будет работать независимо
    receive_thread_.detach();
    
    std::cout << "Started receiving data from server" << std::endl;
}

void SmartClient::disconnect() {
    if (connected_.load()) {
        std::cout << "Disconnecting from server..." << std::endl;
        running_.store(false);
        connected_.store(false);
        
        if (client_socket_.isValid()) {
            close(client_socket_.get());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "Disconnected from server" << std::endl;
    }
}

bool SmartClient::isConnected() const {
    return connected_.load();
}

bool SmartClient::isRunning() const {
    return running_.load();
}

SmartClient::~SmartClient() {
    std::cout << "SmartClient shutting down..." << std::endl;
    disconnect();
    
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    
    std::cout << "SmartClient destroyed" << std::endl;
}