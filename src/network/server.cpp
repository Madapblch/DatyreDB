#include "datyredb/network/server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <iostream>
#include <shared_mutex>
#include <condition_variable>

namespace datyredb::network {

Server::Server(const ServerConfig& config) : config_(config) {
    stats_.start_time = std::chrono::steady_clock::now();
}

Server::~Server() {
    stop();
}

bool Server::start() {
    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[ERROR] Failed to create socket\n";
        return false;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    // Bind
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.tcp_port);
    inet_pton(AF_INET, config_.bind_address.c_str(), &addr.sin_addr);
    
    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[ERROR] Failed to bind to " << config_.bind_address 
                  << ":" << config_.tcp_port << "\n";
        close(server_fd_);
        return false;
    }
    
    // Listen
    if (listen(server_fd_, SOMAXCONN) < 0) {
        std::cerr << "[ERROR] Failed to listen\n";
        close(server_fd_);
        return false;
    }
    
    running_ = true;
    
    // Start accept thread
    accept_thread_ = std::thread(&Server::accept_loop, this);
    
    // Start worker threads
    for (size_t i = 0; i < config_.worker_threads; ++i) {
        worker_threads_.emplace_back(&Server::worker_loop, this);
    }
    
    // Start cleanup thread
    cleanup_thread_ = std::thread(&Server::cleanup_loop, this);
    
    std::cout << "[INFO] DatyreDB Server started on " 
              << config_.bind_address << ":" << config_.tcp_port << "\n";
    
    return true;
}

void Server::stop() {
    running_ = false;
    
    // Close server socket
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    // Wake up workers
    queue_cv_.notify_all();
    
    // Join threads
    if (accept_thread_.joinable()) accept_thread_.join();
    for (auto& t : worker_threads_) {
        if (t.joinable()) t.join();
    }
    if (cleanup_thread_.joinable()) cleanup_thread_.join();
    
    // Close all connections
    std::unique_lock lock(connections_mutex_);
    for (auto& [id, conn] : connections_) {
        conn->close();
    }
    connections_.clear();
    
    std::cout << "[INFO] Server stopped\n";
}

void Server::accept_loop() {
    while (running_) {
        struct pollfd pfd{server_fd_, POLLIN, 0};
        
        int ret = poll(&pfd, 1, 1000);
        if (ret <= 0) continue;
        
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;
        
        // Check connection limit
        if (stats_.active_connections >= config_.max_connections) {
            close(client_fd);
            continue;
        }
        
        // Get client IP
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        std::string client_ip(ip_str);
        
        // Set non-blocking
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        
        // Create connection
        auto conn = std::make_shared<Connection>(
            next_connection_id_++, 
            client_fd, 
            client_ip
        );
        
        {
            std::unique_lock lock(connections_mutex_);
            connections_[conn->get_id()] = conn;
        }
        
        stats_.total_connections++;
        stats_.active_connections++;
        
        // Notify callback
        if (on_connection_) {
            on_connection_(conn);
        }
        
        // Queue for processing
        {
            std::lock_guard lock(queue_mutex_);
            work_queue_.push([this, conn]() {
                handle_client(conn->get_fd(), conn->get_ip());
            });
        }
        queue_cv_.notify_one();
    }
}

void Server::worker_loop() {
    while (running_) {
        std::function<void()> task;
        
        {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !work_queue_.empty() || !running_;
            });
            
            if (!running_ && work_queue_.empty()) return;
            
            if (!work_queue_.empty()) {
                task = std::move(work_queue_.front());
                work_queue_.pop();
            }
        }
        
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Worker exception: " << e.what() << "\n";
            }
        }
    }
}

void Server::cleanup_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        auto now = std::chrono::steady_clock::now();
        std::vector<uint64_t> to_remove;
        
        {
            std::shared_lock lock(connections_mutex_);
            for (const auto& [id, conn] : connections_) {
                auto idle_time = now - conn->get_last_activity();
                if (idle_time > config_.idle_timeout) {
                    to_remove.push_back(id);
                }
            }
        }
        
        for (auto id : to_remove) {
            kill_connection(id);
        }
    }
}

void Server::handle_client(int client_fd, const std::string& client_ip) {
    std::vector<uint8_t> buffer(config_.read_buffer_size);
    
    while (running_) {
        struct pollfd pfd{client_fd, POLLIN, 0};
        int ret = poll(&pfd, 1, 1000);
        
        if (ret < 0) break;
        if (ret == 0) continue;
        
        ssize_t bytes = recv(client_fd, buffer.data(), buffer.size(), 0);
        if (bytes <= 0) break;
        
        stats_.bytes_received += bytes;
        
        // Parse and process message
        // (Protocol parsing logic here)
    }
    
    stats_.active_connections--;
}

size_t Server::get_connection_count() const {
    std::shared_lock lock(connections_mutex_);
    return connections_.size();
}

bool Server::kill_connection(uint64_t connection_id) {
    std::unique_lock lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    if (it == connections_.end()) return false;
    
    it->second->close();
    connections_.erase(it);
    
    stats_.active_connections--;
    
    if (on_disconnect_) {
        on_disconnect_(connection_id);
    }
    
    return true;
}

} // namespace datyredb::network
