// Исправленный инклюд (указываем путь от корня src, который мы настроили в CMake)
#include "network/dispatcher.hpp"

// Теперь нам нужны подробности о Database
#include "core/database.hpp"

#include <sstream>
#include <iostream>

namespace datyre {
namespace network {

    Dispatcher::Dispatcher(datyre::Database& db) : db_(db) {}

    std::string Dispatcher::dispatch(const std::string& request) {
        // Простейшая логика парсинга (Mock)
        // В будущем здесь будет вызов SQL Parser
        
        if (request.empty()) {
            return "Error: Empty command";
        }

        // Логирование (можно заменить на spdlog)
        std::cout << "[Dispatcher] Processing: " << request << std::endl;

        // Псевдо-роутинг команд
        if (request.find("CREATE") == 0) {
            // Тут должен быть вызов db_.execute(...)
            return "ACK: CREATE command received";
        }
        else if (request.find("SELECT") == 0) {
            return "ACK: SELECT command received";
        }
        else if (request == "PING" || request == "ping") {
            return "PONG";
        }

        return "Error: Unknown command";
    }

} // namespace network
} // namespace datyre
