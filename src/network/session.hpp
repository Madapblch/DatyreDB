#pragma once

#include <memory>
#include <string>

// Forward declarations для ускорения компиляции
namespace datyre {
    class Database;
}

namespace datyre {
namespace network {

    class Connection; // Forward declaration

    // Наследуемся от enable_shared_from_this, чтобы безопасно передавать this в колбэки
    class Session : public std::enable_shared_from_this<Session> {
    public:
        // Создаем сессию через фабричный метод (Best Practice для shared_from_this)
        static std::shared_ptr<Session> create(std::shared_ptr<Connection> connection, datyre::Database& db);

        Session(std::shared_ptr<Connection> connection, datyre::Database& db);
        ~Session();

        // Запуск обработки сессии
        void start();

    private:
        std::shared_ptr<Connection> connection_;
        datyre::Database& db_;

        // Метод, который будет вызван, когда придут данные
        void on_message_received(const std::string& message);
    };

} // namespace network
} // namespace datyre
