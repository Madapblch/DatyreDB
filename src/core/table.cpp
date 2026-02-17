#include "datyredb/table.h"
#include "datyredb/status.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace datyredb {

// Конструктор
Table::Table(std::string name) : name_(name) {}

Status Table::insert(const nlohmann::json& data) {
    // 1. Проверяем входные данные на пустоту
    if (data.empty()) {
        return Status::IOError("Attempted to insert empty JSON into table: " + name_);
    }

    // 2. Валидация схемы (твой профессиональный подход)
    // Метод validate() должен быть объявлен в table.h
    if (!validate(data)) {
        return Status::IOError("Invalid data format for table: " + name_);
    }

    // 3. Запись в файл с обработкой исключений
    try {
        // Здесь вызываем твой старый метод сохранения
        saveToFile(data); 
    } 
    catch (const std::exception& e) {
        // Если диск переполнен или нет прав, мы вернем статус, а не "уроним" сервер
        return Status::IOError("Failed to write to disk: " + std::string(e.what()));
    }

    return Status::OK();
}

// Пример реализации вспомогательного метода
bool Table::validate(const nlohmann::json& data) {
    // Твоя логика проверки по схеме
    return true; // Временно
}

} // namespace datyredb