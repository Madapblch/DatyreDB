#include "datyredb/database.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace datyredb {

Database::Database(std::string name) 
    : name_(name), root_path_("./" + name) {}

Status Database::initialize() {
    try {
        // Проверяем, существует ли уже такая папка
        if (fs::exists(root_path_)) {
            // Это не всегда ошибка, но профессионально будет проверить права доступа
            return Status::OK(); 
        }

        // Пытаемся создать директорию
        if (fs::create_directories(root_path_)) {
            return Status::OK();
        } else {
            return Status::IOError("Could not create database directory at " + root_path_.string());
        }
    } catch (const fs::filesystem_error& e) {
        return Status::IOError(std::string("Filesystem error: ") + e.what());
    }
}

Status Database::createTable(const std::string& tableName, const nlohmann::json& schema) {
    if (tableName.empty()) {
        return Status::NotFound("Table name cannot be empty");
    }

    fs::path tablePath = root_path_ / (tableName + ".json");

    if (fs::exists(tablePath)) {
        return Status::AlreadyExists("Table '" + tableName + "' already exists in database '" + name_ + "'");
    }

    // Логика создания файла таблицы...
    // Если всё ок:
    return Status::OK();
}

} // namespace datyredb