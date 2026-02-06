#include "datyredb/network/dispatcher.h"
#include "datyredb/commands/icommand.h"
#include "datyredb/commands/create_command.h" // Тебе нужно будет создать этот файл
#include <iostream>

namespace datyredb {

// Конструктор: инициализируем базу и сразу создаем реестр команд
CommandDispatcher::CommandDispatcher(Database& db) 
    : db_(db), pool_(std::thread::hardware_concurrency()) {
    init_registry();
    std::cout << "[DISPATCHER] Initialized with " << std::thread::hardware_concurrency() << " worker threads." << std::endl;
}

// Регистрация команд: здесь мы наполняем карту (Map) объектами
void CommandDispatcher::init_registry() {
    // Каждая команда — это отдельный класс, наследуемый от ICommand
    registry_["CREATE_TABLE"] = std::make_unique<CreateTableCommand>();
    
    // Сюда в будущем ты добавишь:
    // registry_["INSERT"] = std::make_unique<InsertCommand>();
    // registry_["SELECT"] = std::make_unique<SelectCommand>();
    
    std::cout << "[DISPATCHER] Registered " << registry_.size() << " commands." << std::endl;
}

/**
 * ГЛАВНЫЙ МЕТОД (Асинхронный)
 * Принимает запрос, закидывает в ThreadPool и возвращает "обещание" (future)
 */
std::future<std::string> CommandDispatcher::dispatch(const std::string& raw_json) {
    // Мы используем лямбда-выражение [this, raw_json], чтобы передать контекст в поток
    return pool_.enqueue([this, raw_json]() -> std::string {
        try {
            // 1. Парсим входящий JSON
            auto request = nlohmann::json::parse(raw_json);
            
            // Ожидаем формат: {"command": "NAME", "args": {...}}
            std::string cmd_name = request.value("command", "UNKNOWN");
            nlohmann::json args = request.value("args", nlohmann::json::object());

            // 2. Ищем команду в реестре (Map)
            auto it = registry_.find(cmd_name);
            if (it != registry_.end()) {
                // ВЫПОЛНЕНИЕ: Здесь вызывается метод execute() конкретной команды
                // Это происходит в отдельном потоке из ThreadPool!
                auto [status, payload] = it->second->execute(db_, args);

                // 3. Формируем профессиональный JSON-ответ
                nlohmann::json response;
                response["status"] = status.ok() ? "OK" : "ERROR";
                if (!status.ok()) {
                    response["error_message"] = status.message();
                    response["error_code"] = static_cast<int>(status.code());
                }
                response["results"] = payload;

                return response.dump(); // Превращаем JSON в строку для отправки по сети
            }

            return "{\"status\":\"ERROR\", \"message\":\"Unknown command: " + cmd_name + "\"}";

        } catch (const nlohmann::json::parse_error& e) {
            return "{\"status\":\"ERROR\", \"message\":\"Invalid JSON format\"}";
        } catch (const std::exception& e) {
            return "{\"status\":\"ERROR\", \"message\":\"Internal server error\"}";
        }
    });
}

} // namespace datyredb