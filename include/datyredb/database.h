#pragma once
#include <string>
#include <vector>
#include <filesystem> // Используем современный стандарт для работы с путями
#include "datyredb/status.h"
#include "datyredb/table.h"

namespace datyredb {

class Database {
public:
    // Конструктор теперь просто инициализирует объект
    Database(std::string name);

    // Основной метод для развертывания структуры базы на диске
    // Возвращает Status: успех или описание ошибки
    Status initialize();

    // Создание таблицы
    Status createTable(const std::string& tableName, const nlohmann::json& schema);

private:
    std::string name_;
    std::filesystem::path root_path_;
};

} // namespace datyredb