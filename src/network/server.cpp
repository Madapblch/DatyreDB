#include "network/server.hpp"
#include "network/connection.hpp"

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>
#include <mutex>
#include <shared_mutex>

namespace datyre {
namespace network {

    Server::Server(const ServerConfig& config) : config_(config) {
        // Конструктор
    }

    Server::~Server() {
        stop();
    }

    bool Server::start() {
        if (running_) return true;

        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            perror("Socket creation failed");
            return false;
        }

        int opt = 1;
        if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("Setsockopt failed");
            close(server_fd_);
            return false;
        }

        int flags = fcntl(server_fd_, F_GETFL, 0);
        if (flags != -1) {
            fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);
        }

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(config_.tcp_port);

        if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("Bind failed");
            close(server_fd_);
            return false;
        }

        if (listen(server_fd_, config_.max_connections) < 0) {
            perror("Listen failed");
            close(server_fd_);
            return false;
        }

        running_ = true;
        
        accept_thread_ = std::thread(&Server::accept_loop, this);
        
        for (int i = 0; i < 4; ++i) {
            worker_threads_.emplace_back(&Server::worker_loop, this);
        }

        cleanup_thread_ = std::thread(&Server::cleanup_loop, this);

        std::cout << "[Server] Started on port " << config_.tcp_port << std::endl;
        return true;
    }

    void Server::stop() {
        running_ = false;
        
        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }

        queue_cv_.notify_all();

        if (accept_thread_.joinable()) accept_thread_.join();
        
        for (auto& t : worker_threads_) {
            if (t.joinable()) t.join();
        }
        
        if (cleanup_thread_.joinable()) cleanup_thread_.join();

        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        connections_.clear();
    }

    void Server::accept_loop() {
        while (running_) {
            struct pollfd pfd{server_fd_, POLLIN, 0};
            int ret = poll(&pfd, 1, 500);

            if (ret > 0 && (pfd.revents & POLLIN)) {
                struct sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);

                if (client_fd >= 0) {
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
                    
                    if (stats_.active_connections >= static_cast<size_t>(config_.max_connections)) {
                        close(client_fd);
                    } else {
                        handle_client(client_fd, client_ip);
                    }
                }
            }
        }
    }

    void Server::handle_client(int client_fd, [[maybe_unused]] const std::string& client_ip) {
        int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags != -1) {
            fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        }

        auto conn = std::make_shared<Connection>(client_fd);

	std::string welcome_msg = "Connected to DatyreDB v1.0\nReady for commands (Try: PING, SELECT...)\n";
        conn->send_data(welcome_msg);

        {
            std::unique_lock<std::shared_mutex> lock(connections_mutex_);
            connections_[next_connection_id_] = conn;
            next_connection_id_++;
        }

        stats_.active_connections++;
        stats_.total_connections++;

        if (on_connection_) {
            on_connection_(next_connection_id_ - 1);
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            work_queue_.push([conn]() {
                conn->start();
            });
        }
        queue_cv_.notify_one();
    }

    void Server::worker_loop() {
        while (running_) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this] {
                    return !work_queue_.empty() || !running_;
                });

                if (!running_ && work_queue_.empty()) return;
                
                if (!work_queue_.empty()) {
                    task = std::move(work_queue_.front());
                    work_queue_.pop();
                }
            }
            if (task) task();
        }
    }

    void Server::cleanup_loop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    size_t Server::get_connection_count() const {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        return connections_.size();
    }

    bool Server::kill_connection(uint64_t connection_id) {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(connection_id);
        if (it != connections_.end()) {
            connections_.erase(it);
            stats_.active_connections--;
            if (on_disconnect_) on_disconnect_(connection_id);
            return true;
        }
        return false;
    }

} // namespace network
} // namespace datyre
