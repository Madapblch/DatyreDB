#pragma once

#include <memory>
#include <string>
#include <boost/asio.hpp>
#include <deque>

namespace datyre {
    class Database;
}

namespace datyre {
namespace network {

    using boost::asio::ip::tcp;

    class Session : public std::enable_shared_from_this<Session> {
    public:
        static std::shared_ptr<Session> create(tcp::socket socket, datyre::Database& db);

        Session(tcp::socket socket, datyre::Database& db);
        
        void start();
        
        // Исправлено: передача по значению (by value), так как внутри мы меняем строку
        void deliver(std::string msg);

    private:
        tcp::socket socket_;
        datyre::Database& db_;
        
        boost::asio::streambuf input_buffer_;
        std::deque<std::string> write_msgs_;

        void do_read();
        void do_write();
        void process_command(std::string command);
    };

} // namespace network
} // namespace datyre
