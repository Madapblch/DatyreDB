#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <shared_mutex> // <--- ВОТ ЭТОГО НЕ ХВАТАЛО! БЕЗ НЕГО ВСЁ ПАДАЕТ.

namespace datyre {
    class Database;
    namespace network {
        class Connection;
    }
}

namespace datyre {
namespace network {

    struct ServerConfig {
        int tcp_port = 7432;
        int max_connections = 1000;
        int read_buffer_size = 4096;
        int idle_timeout = 60;
    };

    struct ServerStats {
        std::atomic<size_t> active_connections{0};
        std::atomic<size_t> total_connections{0};
        std::atomic<size_t> bytes_received{0};
    };

    class Server {
    public:
        explicit Server(const ServerConfig& config);
        ~Server();

        bool start();
        void stop();

        size_t get_connection_count() const;
        bool kill_connection(uint64_t connection_id);

        std::function<void(uint64_t)> on_connection_;
        std::function<void(uint64_t)> on_disconnect_;

    private:
        ServerConfig config_;
        ServerStats stats_;
        
        int server_fd_ = -1;
        std::atomic<bool> running_{false};

        std::thread accept_thread_;
        std::vector<std::thread> worker_threads_;
        std::thread cleanup_thread_;

        std::queue<std::function<void()>> work_queue_;
        std::mutex queue_mutex_;
        std::condition_variable queue_cv_;

        std::map<uint64_t, std::shared_ptr<Connection>> connections_;
        mutable std::shared_mutex connections_mutex_;
        uint64_t next_connection_id_ = 1;

        void accept_loop();
        void worker_loop();
        void cleanup_loop();
        
        void handle_client(int client_fd, const std::string& client_ip);
    };

} // namespace network
} // namespace datyre
