#pragma once

#include <string>
#include <chrono>
#include <atomic>
#include <memory>

namespace datyredb::network {

class Connection {
public:
    Connection(uint64_t id, int fd, const std::string& ip)
        : id_(id), fd_(fd), client_ip_(ip) {
        last_activity_ = std::chrono::steady_clock::now();
    }
    
    ~Connection() { close(); }
    
    uint64_t get_id() const { return id_; }
    int get_fd() const { return fd_; }
    const std::string& get_ip() const { return client_ip_; }
    
    std::chrono::steady_clock::time_point get_last_activity() const {
        return last_activity_;
    }
    
    void update_activity() {
        last_activity_ = std::chrono::steady_clock::now();
    }
    
    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    
    bool is_closed() const { return fd_ < 0; }
    
private:
    uint64_t id_;
    int fd_;
    std::string client_ip_;
    std::chrono::steady_clock::time_point last_activity_;
};

} // namespace datyredb::network
