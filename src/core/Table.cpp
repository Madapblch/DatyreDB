#include "datyredb/status.h"

datyredb::Status Table::insert(const JSON& data) {
    // 1. Проверяем схему
    if (!validate(data)) {
        return Status::IOError("Invalid data format for table: " + this->name);
    }

    // 2. Пытаемся записать в файл
    try {
        saveToFile(data);
    } catch (const std::exception& e) {
        return Status::IOError("Failed to write to disk: " + std::string(e.what()));
    }

    return Status::OK(); // Все прошло успешно
}