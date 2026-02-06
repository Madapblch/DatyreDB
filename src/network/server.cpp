#include "datyredb/network/server.h"
#include <iostream>
#include <vector>

namespace datyredb {

Server::Server(uint16_t port, Database& db) 
    : port_(port), db_(db), dispatcher_(db) {
    // Диспетчер при создании сам проинициализирует ThreadPool и реестр команд
}

void Server::run() {
    try {
        // Создаем "принимальщика" соединений на указанном порту
        asio::ip::tcp::acceptor acceptor(io_context_, 
            asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port_));

        std::cout << "[SERVER] DatyreDB Network Service started." << std::endl;
        std::cout << "[SERVER] Listening on TCP port: " << port_ << std::endl;

        while (true) {
            // Ожидаем клиента (блокирующая операция)
            asio::ip::tcp::socket socket(io_context_);
            acceptor.accept(socket);

            // Как только клиент подключился — обрабатываем его
            handle_client(std::move(socket));
        }
    } catch (const std::exception& e) {
        std::cerr << "[SERVER] Critical error in run loop: " << e.what() << std::endl;
    }
}

void Server::handle_client(asio::ip::tcp::socket socket) {
    try {
        // Читаем данные от клиента (упрощенный буфер на 1024 байта)
        std::vector<char> buffer(1024);
        std::size_t length = socket.read_some(asio::buffer(buffer));
        
        if (length > 0) {
            std::string request(buffer.data(), length);
            std::cout << "[SERVER] Received request: " << request << std::endl;

            // --- КЛЮЧЕВОЙ МОМЕНТ СВЯЗИ ---
            // 1. Отдаем запрос Диспетчеру. Он вернет std::future.
            auto future_response = dispatcher_.dispatch(request);

            // 2. Ждем выполнения задачи в ThreadPool (профи-подход)
            // get() дождется завершения команды в другом потоке и вернет JSON-строку
            std::string response = future_response.get();

            // 3. Отправляем результат обратно клиенту
            asio::write(socket, asio::buffer(response + "\n"));
            
            std::cout << "[SERVER] Task completed. Response sent." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[SERVER] Client handling error: " << e.what() << std::endl;
    }
    // Сокет закроется автоматически при выходе из области видимости
}

} // namespace datyredb