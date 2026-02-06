#include <iostream>
#include "datyredb/database.h"
#include "datyredb/status.h"

int main() {
    datyredb::Database db("UsersDB");

    // Инициализируем базу и проверяем статус
    auto status = db.initialize();
    
    if (!status.ok()) {
        // Профессиональный подход: выводим ошибку в cerr и завершаем работу
        std::cerr << "CRITICAL FAILURE: " << status.toString() << std::endl;
        return 1;
    }

    std::cout << "Database initialized successfully." << std::endl;

    // Пытаемся создать таблицу
    auto tableStatus = db.createTable("admins", { /* schema json */ });
    if (!tableStatus.ok()) {
        std::cout << "Warning: " << tableStatus.message() << std::endl;
    }

    return 0;
}