#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>

// Forward Declaration: Снижаем связность кода.
// Серверу нужно знать про Database, но не нужно тянуть весь заголовок database.hpp сюда.
namespace datyre {
    class Database;
}

namespace datyre {
namespace network {

    class HttpServer {
    public:
        // Конструктор принимает ссылку на базу и порт
        HttpServer(datyre::Database& db, int port);
        
        // Деструктор (гарантирует остановку потока)
        ~HttpServer();

        // Запуск сервера (неблокирующий)
        void start();

        // Остановка сервера
        void stop();

    private:
        datyre::Database& db_;
        int port_;
        
        // Атомарный флаг для безопасной остановки потоков
        std::atomic<bool> is_running_;
        
        // Поток, в котором крутится сервер
        std::thread server_thread_;

        // Внутренний метод цикла обработки
        void run_event_loop();
    };

} // namespace network
} // namespace datyre
