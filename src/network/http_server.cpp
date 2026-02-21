// 1. Подключаем правильный заголовок (который мы только что создали)
#include "network/http_server.hpp"

// 2. Теперь можно подключить Database, так как в cpp нам нужна реализация
#include "core/database.hpp"

#include <iostream>
#include <chrono>

namespace datyre {
namespace network {

    HttpServer::HttpServer(datyre::Database& db, int port)
        : db_(db), port_(port), is_running_(false) {
    }

    HttpServer::~HttpServer() {
        stop();
    }

    void HttpServer::start() {
        if (is_running_) return;

        is_running_ = true;
        std::cout << "[HttpServer] Starting on port " << port_ << "..." << std::endl;

        // Запускаем цикл в отдельном потоке
        server_thread_ = std::thread(&HttpServer::run_event_loop, this);
    }

    void HttpServer::stop() {
        if (!is_running_) return;
        
        is_running_ = false;
        std::cout << "[HttpServer] Stopping..." << std::endl;

        // Ждем завершения потока (Graceful Shutdown)
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    void HttpServer::run_event_loop() {
        // Эмуляция работы сервера
        // В реальности тут будет while(accept(...))
        while (is_running_) {
            // Чтобы поток не грузил CPU впустую в нашей заглушке
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

} // namespace network
} // namespace datyre
