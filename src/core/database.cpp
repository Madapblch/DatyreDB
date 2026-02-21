#include "core/database.hpp"
#include <iostream>
#include <sstream>

namespace datyre {

    Database::Database() : data_dir_("./data") {
        // Здесь можно загружать таблицы с диска
        // load_tables();
        std::cout << "[Database] Initialized." << std::endl;
    }

    Database::~Database() {
        shutdown();
    }

    void Database::shutdown() {
        std::cout << "[Database] Shutting down..." << std::endl;
        // Здесь логика сброса данных на диск
    }

    std::string Database::execute(const std::string& query) {
        // В будущем здесь будет вызов SQL Parser
        // auto ast = parser.parse(query);
        // executor.execute(ast);
        
        // Заглушка для тестов
        return "Executed: " + query;
    }

    Status Database::CreateTable(const std::string& name, const std::vector<std::string>& columns) {
        std::unique_lock lock(tables_mutex_); // Блокируем на запись
        
        if (tables_.find(name) != tables_.end()) {
            return Status::InvalidArgument("Table already exists");
        }

        // Создаем новую таблицу
        // (Предполагается, что конструктор Table принимает имя)
        auto table = std::make_shared<Table>(name);
        
        // TODO: Добавить колонки в таблицу
        (void)columns; // Чтобы не было warning

        tables_[name] = table;
        return Status::OK();
    }

    std::shared_ptr<Table> Database::GetTable(const std::string& name) {
        std::shared_lock lock(tables_mutex_); // Блокируем на чтение (несколько потоков могут читать одновременно)
        
        auto it = tables_.find(name);
        if (it != tables_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void Database::load_tables() {
        // Логика загрузки с диска
    }

} // namespace datyre
