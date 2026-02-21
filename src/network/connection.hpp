#pragma once

#include <memory>
#include <string>
#include <netinet/in.h>

namespace datyre {
namespace network {

    // Наследуемся от enable_shared_from_this для безопасного использования в async коде
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        explicit Connection(int socket_fd);
        ~Connection();

        // Запрещаем копирование, чтобы не закрыть сокет дважды
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        void start();
        void send_data(const std::string& data);
        
        int get_fd() const { return socket_fd_; }
        std::string get_ip() const { return "127.0.0.1"; } // Заглушка, можно расширить
        uint64_t get_id() const { return id_; }
        void set_id(uint64_t id) { id_ = id; }

    private:
        int socket_fd_;
        uint64_t id_ = 0;
        bool is_active_;

        void handle_read();
    };

} // namespace network
} // namespace datyre
