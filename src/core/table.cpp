// 1. Сначала заголовок этого класса
#include "core/table.hpp"

// 2. Сторонние библиотеки
#include <nlohmann/json.hpp>
#include <iostream>

// 3. Внутренние зависимости
#include "datyredb/status.hpp"

namespace datyre {

    // Реализация конструктора
    Table::Table(std::string name) : name_(std::move(name)) {}

    // Реализация insert
    Status Table::insert(const nlohmann::json& data) {
        if (!validate(data)) {
            return Status::InvalidArgument("Data validation failed");
        }
        
        // Тут логика вставки...
        // rows_.push_back(data); 

        return Status::OK();
    }

    // Реализация validate
    bool Table::validate(const nlohmann::json& data) {
        // Заглушка проверки
        if (data.empty()) return false;
        return true;
    }

} // namespace datyre
