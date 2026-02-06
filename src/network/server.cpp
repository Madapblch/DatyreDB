#include "datyredb/server.h"
#include "datyredb/status.h"
#include <iostream>

namespace datyredb {

Server::Server(uint16_t port, Database& db) 
    : port_(port), db_(db) {}

void Server::run() {
    asio::ip::tcp::acceptor acceptor(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port_));
    std::cout << "[SERVER] DatyreDB listening on port " << port_ << "..." << std::endl;

    while (true) {
        // Ждем клиента
        asio::ip::tcp::socket socket(io_context_);
        acceptor.accept(socket);
        
        // В проф версии тут должен быть std::thread или асинхронность
        handle_client(std::move(socket));
    }
}

void Server::handle_client(asio::ip::tcp::socket socket) {
    try {
        char data[1024];
        std::size_t length = socket.read_some(asio::buffer(data));
        std::string request(data, length);

        std::cout << "[SERVER] Received: " << request << std::endl;

        // ЗДЕСЬ СВЯЗЬ С ТВОИМ ЯДРОМ:
        // Например, простейший парсер: если пришло "CREATE name", создаем таблицу
        // В будущем тут будет полноценный парсер команд
        std::string response;
        if (request.find("PING") == 0) {
            response = "PONG\n";
        } else {
            response = "ERROR: Unknown command\n";
        }

        asio::write(socket, asio::buffer(response));
    } catch (std::exception& e) {
        std::cerr << "[SERVER] Client error: " << e.what() << std::endl;
    }
}

} // namespace datyredb