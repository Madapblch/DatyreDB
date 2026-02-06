#include <iostream>
#include <datyredb/database.h> // Путь к твоему будущему интерфейсу
#include <datyredb/status.h>

int main() {
    // 1. Инициализация базы данных в локальной папке
    datyredb::Database db("./my_local_db");

    // 2. Создание таблицы со схемой
    // В профи СУБД мы проверяем статус операции, а не просто верим, что она прошла
    auto status = db.createTable("users", {
        {"id", datyredb::Type::INT},
        {"name", datyredb::Type::STRING},
        {"is_admin", datyredb::Type::BOOL}
    });

    if (status != datyredb::Status::OK) {
        std::cerr << "Ошибка при создании таблицы: " << status.message() << std::endl;
        return 1;
    }

    // 3. Вставка данных
    db.table("users").insert({
        {"id", 1},
        {"name", "Admin"},
        {"is_admin", true}
    });

    std::cout << "База данных успешно инициализирована локально!" << std::endl;
    return 0;
}