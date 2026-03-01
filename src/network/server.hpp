#pragma once

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <memory>

namespace datyre { class Database; }

namespace datyre {
namespace network {

    using boost::asio::ip::tcp;

    struct ServerConfig {
        int tcp_port = 7432;
        int max_connections = 1000;
    };

    class Server {
    public:
        Server(const ServerConfig& config, datyre::Database& db);
        
        // Запуск Event Loop (блокирует поток)
        void run();
        
        // Остановка
        void stop();

    private:
        void do_accept();

        boost::asio::io_context io_context_;
        tcp::acceptor acceptor_;
        datyre::Database& db_;
        bool running_ = false;
    };

} // namespace network
} // namespace datyre
