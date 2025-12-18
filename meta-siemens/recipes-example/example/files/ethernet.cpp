#include <iostream>
#include <memory>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <chrono>
#include <sys/ioctl.h>
#include <net/if.h>

#include "ethernet.hpp"

static void socket_deleter(int* fd) {
    if (fd && *fd >= 0) {
        ::close(*fd);
        std::cout << "[ETHERNET] Socket closed: " << *fd << std::endl;
    }
    delete fd;
}

SmartSocket::SmartSocket() 
    : socket_fd_(new int(-1), socket_deleter) {}

SmartSocket::SmartSocket(int fd) 
    : socket_fd_(new int(fd), socket_deleter) {}

bool SmartSocket::create() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "[ETHERNET] Failed to create socket: " 
                  << strerror(errno) << std::endl;
        return false;
    }
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

void SmartSocket::close() {
    if (socket_fd_ && *socket_fd_ >= 0) {
        ::close(*socket_fd_);
        socket_fd_.reset(new int(-1));
    }
}

void SmartClient::setupLed(SysfsLedController* led1, SysfsLedController* led2) {
    led1_ = led1;
    led2_ = led2;
}

SmartClient::SmartClient() {
    signal(SIGPIPE, SIG_IGN);
    socket_ = std::make_unique<SmartSocket>();
}

SmartClient::~SmartClient() {
    stop();
}

bool SmartClient::checkConnection() {
    if (!socket_->isValid()) {
        return false;
    }
    
    // Проверка через getsockopt
    int error = 0;
    socklen_t len = sizeof(error);
    
    if (getsockopt(socket_->get(), SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        return false;
    }
    
    return (error == 0);
}

void SmartClient::cleanup() {
    if (socket_->isValid()) {
        shutdown(socket_->get(), SHUT_RDWR);
        socket_->close();
    }
    socket_.reset(new SmartSocket());
    connected_ = false;
    running_ = false;
}

bool SmartClient::start(const std::string& ip, int port) {
    // Если уже работает - останавливаем
    if (running_) {
        stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "[ETHERNET] Starting client..." << std::endl;
    
    // Создаем сокет
    if (!socket_->create()) {
        std::cerr << "[ETHERNET] Failed to create socket" << std::endl;
        return false;
    }
    
    // Настраиваем адрес сервера
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "[ETHERNET] Invalid IP address: " << ip << std::endl;
        cleanup();
        return false;
    }
    
    // Устанавливаем таймауты (10 секунд)
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    
    setsockopt(socket_->get(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket_->get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Включаем keepalive
    int keepalive = 1;
    setsockopt(socket_->get(), SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    
    // Подключаемся
    std::cout << "[ETHERNET] Connecting to " << ip << ":" << port << "..." << std::endl;
    
    if (::connect(socket_->get(), (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "[ETHERNET] Connection failed: " << strerror(errno) << std::endl;
        cleanup();
        return false;
    }
    
    connected_ = true;
    running_ = true;
    
    // Сбрасываем счетчик
    message_counter_ = 0;
    
    // Запускаем потоки
    sender_thread_ = std::thread(&SmartClient::sendingLoop, this);
    receiver_thread_ = std::thread(&SmartClient::receivingLoop, this);
    
    std::cout << "[ETHERNET] Client started successfully!" << std::endl;
    return true;
}

void SmartClient::sendingLoop() {
    std::cout << "[ETHERNET] Sender thread started" << std::endl;
    
    int failed_heartbeats = 0;
    const int MAX_FAILED_HEARTBEATS = 3;
    
    while (running_ && connected_) {
        std::vector<uint8_t> data_to_send;
        bool has_data = false;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Ждем 2 секунды или пока появится сообщение
            if (queue_cv_.wait_for(lock, std::chrono::seconds(2),
                [this] { return !send_queue_.empty() || !running_; })) {
                
                if (!running_) break;
                
                if (!send_queue_.empty()) {
                    data_to_send = std::move(send_queue_.front());
                    send_queue_.pop();
                    has_data = true;
                    std::cout << "[ETHERNET] Got data from queue (" 
                              << data_to_send.size() << " bytes)" << std::endl;
                }
            }
        }
        
        if (!running_ || !connected_) break;
        
        if (has_data) {
            // Отправляем данные из очереди
            if (!sendDataInternal(data_to_send)) {
                std::cerr << "[ETHERNET] Failed to send queued data" << std::endl;
                connected_ = false;
                running_ = false;
                break;
            }
            else{
                const unsigned int hz=4;
                led2_->blink(hz); // rk_func_communication_confirmation
            }
            continue; // Переходим к следующей итерации
        }
        
        // Если очередь пуста - heartbeat
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        if (!running_ || !connected_) break;
        
        // Heartbeat сообщение
        int counter = ++message_counter_;
        std::string message = "PING#" + std::to_string(counter) + "\n";
        
        // Отправляем с коротким таймаутом
        struct timeval tv = {2, 0}; // 2 секунды таймаут
        setsockopt(socket_->get(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        
        ssize_t sent = send(socket_->get(), message.c_str(), message.length(), MSG_NOSIGNAL);
        
        // Восстанавливаем таймаут
        tv.tv_sec = 10;
        setsockopt(socket_->get(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        
        if (sent < 0) {
            failed_heartbeats++;
            std::cerr << "[ETHERNET] Heartbeat failed (" << failed_heartbeats 
                      << "/" << MAX_FAILED_HEARTBEATS << "): " << strerror(errno) << std::endl;
            
            if (failed_heartbeats >= MAX_FAILED_HEARTBEATS) {
                std::cerr << "[ETHERNET] Too many failed heartbeats, connection dead!" << std::endl;
                connected_ = false;
                running_ = false;
                break;
            }
        } else if (sent == 0) {
            std::cerr << "[ETHERNET] Connection closed by server" << std::endl;
            connected_ = false;
            running_ = false;
            break;
        } else {
            // Успешная отправка - сбрасываем счетчик
            if (failed_heartbeats > 0) {
                failed_heartbeats = 0;
                std::cout << "[ETHERNET] Heartbeat recovered" << std::endl;
            }
            const unsigned int hz=4;
            led2_->blink(hz); // rk_func_communication_confirmation
            std::cout << "[ETHERNET] Heartbeat #" << counter << " sent" << std::endl;
        }
    }
    
    std::cout << "[ETHERNET] Sender thread stopped" << std::endl;
}

void SmartClient::receivingLoop() {
    std::cout << "[ETHERNET] Receiver thread started" << std::endl;
    
    char buffer[1024];
    
    while (running_ && connected_) {
        if (!running_ || !connected_) break;
        
        memset(buffer, 0, sizeof(buffer));
        
        // Проверяем соединение перед приемом
        if (!checkConnection()) {
            std::cerr << "[ETHERNET] Connection lost in receiver" << std::endl;
            connected_ = false;
            running_ = false;
            break;
        }
        
        // Пытаемся принять данные
        ssize_t received = recv(socket_->get(), buffer, sizeof(buffer) - 1, 0);
        
        if (received > 0) {
            buffer[received] = '\0';
            std::cout << "[ETHERNET] Received " << received << " bytes: " << buffer << std::endl;
            const unsigned int hz=4;
            led2_->blink(hz); // rk_func_communication_confirmation
        } else if (received == 0) {
            std::cout << "[ETHERNET] Server disconnected" << std::endl;
            connected_ = false;
            running_ = false;
            break;
        } else {
            int err = errno;
            
            if (err == EAGAIN || err == EWOULDBLOCK) {
                // Таймаут - нормально
                continue;
            } else if (err == ECONNRESET || err == EPIPE || err == ENOTCONN) {
                std::cerr << "[ETHERNET] Connection error in receiver: " << strerror(err) << std::endl;
                connected_ = false;
                running_ = false;
                break;
            } else {
                std::cerr << "[ETHERNET] Receive error: " << strerror(err) << std::endl;
                // Не разрываем соединение при других ошибках
            }
        }
        
        // Короткая пауза
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "[ETHERNET] Receiver thread stopped" << std::endl;
}

void SmartClient::stop() {
    if (!running_) return;
    
    std::cout << "[ETHERNET] Stopping client..." << std::endl;
    
    running_ = false;
    connected_ = false;
    
    // Даем время потокам завершиться
    if (sender_thread_.joinable()) {
        sender_thread_.join();
        std::cout << "[ETHERNET] Sender thread joined" << std::endl;
    }
    
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
        std::cout << "[ETHERNET] Receiver thread joined" << std::endl;
    }
    
    // Очищаем сокет
    cleanup();
    
    std::cout << "[ETHERNET] Client stopped" << std::endl;
}

bool SmartClient::isRunning() const {
    return running_;
}

bool SmartClient::isConnected() const {
    if (!running_ || !connected_ || !socket_->isValid()) {
        return false;
    }
    
    int socket_error = 0;
    socklen_t len = sizeof(socket_error);
    
    if (getsockopt(socket_->get(), SOL_SOCKET, SO_ERROR, &socket_error, &len) < 0) {
        // Не удалось проверить сокет - считаем что соединение разорвано
        return false;
    }
    
    if (socket_error != 0) {
        // Сокет имеет ошибку (ECONNRESET, EPIPE и т.д.)
        return false;
    }
    
    struct timeval tv = {0, 10000}; // 10ms таймаут
    fd_set read_fds, except_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&except_fds);
    FD_SET(socket_->get(), &read_fds);
    FD_SET(socket_->get(), &except_fds);
    
    int result = select(socket_->get() + 1, &read_fds, nullptr, &except_fds, &tv);
    
    if (result < 0) {
        // Ошибка select
        return false;
    }
    
    if (FD_ISSET(socket_->get(), &except_fds)) {
        // Исключительная ситуация на сокете
        return false;
    }
    
    return true;
}

bool SmartClient::sendData(const std::vector<uint8_t>& data) {
    if (!isConnected()) {
        std::cerr << "[ETHERNET] Cannot send: not connected" << std::endl;
        return false;
    }
    
    if (data.empty()) {
        std::cout << "[ETHERNET] Warning: trying to send empty data" << std::endl;
        return true;
    }
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        send_queue_.push(data);
    }
    
    // Будим поток отправки
    queue_cv_.notify_one();
    
    std::cout << "[ETHERNET] Data queued for sending (" << data.size() << " bytes)" << std::endl;
    return true;
}

bool SmartClient::sendDataInternal(const std::vector<uint8_t>& data) {
    if (!connected_ || !socket_->isValid() || data.empty()) {
        return false;
    }
    
    // Устанавливаем таймаут отправки
    struct timeval tv = {5, 0};
    setsockopt(socket_->get(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    // Отправляем данные
    ssize_t total_sent = 0;
    const uint8_t* buffer = data.data();
    size_t total_size = data.size();
    
    while (total_sent < total_size) {
        ssize_t sent = send(socket_->get(), 
                           buffer + total_sent, 
                           total_size - total_sent, 
                           MSG_NOSIGNAL);
        
        if (sent < 0) {
            int err = errno;
            if (err == EAGAIN || err == EWOULDBLOCK) {
                continue;
            }
            std::cerr << "[ETHERNET] Send error: " << strerror(err) << std::endl;
            return false;
        } else if (sent == 0) {
            std::cerr << "[ETHERNET] Connection closed during send" << std::endl;
            return false;
        }
        
        total_sent += sent;
    }
    
    // Восстанавливаем таймаут
    tv.tv_sec = 10;
    setsockopt(socket_->get(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    std::cout << "[ETHERNET] Successfully sent " << total_size << " bytes" << std::endl;
    return true;
}

int SmartClient::getMessageCount() const {
    return message_counter_;
}

bool SmartClient::checkEthernetLink() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
    
    // Получаем флаги интерфейса
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        close(sock);
        return false;
    }
    
    bool is_up = (ifr.ifr_flags & IFF_UP) != 0;
    bool is_running = (ifr.ifr_flags & IFF_RUNNING) != 0;
    
    close(sock);
    return is_up && is_running;
}