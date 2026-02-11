#pragma once

#include <string>
#include <chrono>
#include <cstdint>

namespace datyredb::network {

struct SessionInfo {
    uint64_t session_id;
    uint64_t connection_id;
    std::string username;
    std::string client_ip;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_activity;
    bool authenticated{false};
};

class Session {
public:
    explicit Session(uint64_t id) : info_{} {
        info_.session_id = id;
        info_.created_at = std::chrono::steady_clock::now();
        info_.last_activity = info_.created_at;
    }
    
    uint64_t get_id() const { return info_.session_id; }
    const SessionInfo& get_info() const { return info_; }
    
    void set_username(const std::string& username) {
        info_.username = username;
    }
    
    void set_authenticated(bool auth) {
        info_.authenticated = auth;
    }
    
    bool is_authenticated() const {
        return info_.authenticated;
    }
    
    void update_activity() {
        info_.last_activity = std::chrono::steady_clock::now();
    }
    
private:
    SessionInfo info_;
};

} // namespace datyredb::network
