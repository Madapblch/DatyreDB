#pragma once

#include "types.h"
#include "connection.h"
#include "session.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>
#include <queue>

namespace datyredb::network {

class Server {
public:
    explicit Server(const ServerConfig& config);
    ~Server();
    
    // Server lifecycle
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    // Connection management
    size_t get_connection_count() const;
    std::vector<SessionInfo> get_active_sessions() const;
    bool kill_connection(uint64_t connection_id);
    
    // Statistics
    struct Statistics {
        std::atomic<uint64_t> total_connections{0};
        std::atomic<uint64_t> active_connections{0};
        std::atomic<uint64_t> total_queries{0};
        std::atomic<uint64_t> failed_queries{0};
        std::atomic<uint64_t> bytes_received{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::chrono::steady_clock::time_point start_time;
    };
    
    const Statistics& get_statistics() const { return stats_; }
    
    // Event callbacks
    using OnConnectionCallback = std::function<void(std::shared_ptr<Connection>)>;
    using OnDisconnectCallback = std::function<void(uint64_t)>;
    using OnQueryCallback = std::function<QueryResult(const std::string&, Session&)>;
    
    void set_on_connection(OnConnectionCallback cb) { on_connection_ = std::move(cb); }
    void set_on_disconnect(OnDisconnectCallback cb) { on_disconnect_ = std::move(cb); }
    void set_on_query(OnQueryCallback cb) { on_query_ = std::move(cb); }
    
private:
    void accept_loop();
    void worker_loop();
    void cleanup_loop();
    
    void handle_client(int client_fd, const std::string& client_ip);
    
    ServerConfig config_;
    int server_fd_{-1};
    std::atomic<bool> running_{false};
    
    // Thread management
    std::thread accept_thread_;
    std::vector<std::thread> worker_threads_;
    std::thread cleanup_thread_;
    
    // Connection management
    mutable std::shared_mutex connections_mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<Connection>> connections_;
    std::atomic<uint64_t> next_connection_id_{1};
    
    // Work queue
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<std::function<void()>> work_queue_;
    
    // Callbacks
    OnConnectionCallback on_connection_;
    OnDisconnectCallback on_disconnect_;
    OnQueryCallback on_query_;
    
    // Statistics
    Statistics stats_;
};

} // namespace datyredb::network
