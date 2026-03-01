#include "network/server.hpp"
#include "network/session.hpp"
#include <iostream>

namespace datyre {
namespace network {

    // Реализация конструктора
    Server::Server(const ServerConfig& config, datyre::Database& db)
        : io_context_(),
          // Инициализация акцептора (слушателя порта)
          acceptor_(io_context_, tcp::endpoint(tcp::v4(), config.tcp_port)),
          db_(db) {
        
        // Сразу начинаем ждать подключений
        do_accept();
    }

    // Запуск Event Loop
    void Server::run() {
        std::cout << "[Server] Async Server running on port " 
                  << acceptor_.local_endpoint().port() << "..." << std::endl;
        running_ = true;
        
        // Этот вызов блокирует поток до тех пор, пока сервер не остановят
        io_context_.run();
    }

    // Остановка сервера
    void Server::stop() {
        if (running_) {
            io_context_.stop();
            running_ = false;
            std::cout << "[Server] Stopped." << std::endl;
        }
    }

    // ВНИМАНИЕ: Вот здесь была ошибка. Добавлено Server::
    void Server::do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::cout << "[Server] New connection accepted: " 
                              << socket.remote_endpoint().address().to_string() << std::endl;
                    
                    // Создаем сессию через фабричный метод и запускаем её
                    // std::move(socket) передает владение сокетом в сессию
                    Session::create(std::move(socket), db_)->start();
                } else {
                    std::cerr << "[Server] Accept error: " << ec.message() << std::endl;
                }

                // Рекурсивный вызов (на самом деле не рекурсия, а постановка в очередь)
                // чтобы принимать следующие подключения
                do_accept();
            });
    }

} // namespace network
} // namespace datyre
