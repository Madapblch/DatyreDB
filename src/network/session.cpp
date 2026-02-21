// 1. Правильный инклюд (без datyredb/, расширение .hpp)
#include "network/session.hpp"

// 2. Зависимости
#include "network/connection.hpp"
#include "network/dispatcher.hpp"
#include "core/database.hpp"

#include <iostream>

namespace datyre {
namespace network {

    // Фабричный метод
    std::shared_ptr<Session> Session::create(std::shared_ptr<Connection> connection, datyre::Database& db) {
        return std::make_shared<Session>(connection, db);
    }

    Session::Session(std::shared_ptr<Connection> connection, datyre::Database& db)
        : connection_(std::move(connection)), db_(db) {
    }

    Session::~Session() {
        std::cout << "[Session] Destroyed." << std::endl;
    }

    void Session::start() {
        // В реальном асинхронном сервере (Boost.Asio) здесь мы бы подписались на события.
        // В нашем синхронном примере Connection сам читает данные.
        // Но архитектурно правильно, чтобы Сессия управляла процессом.
        
        if (connection_) {
            connection_->start();
            // TODO: Здесь должна быть привязка callback-а: connection_->set_callback(...)
        }
    }

    void Session::on_message_received(const std::string& message) {
        // 1. Создаем диспетчер (или используем готовый)
        Dispatcher dispatcher(db_);

        // 2. Обрабатываем запрос
        std::string response = dispatcher.dispatch(message);

        // 3. Отправляем ответ клиенту
        if (connection_) {
            connection_->send_data(response);
        }
    }

} // namespace network
} // namespace datyre
