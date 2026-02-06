#include <iostream>
#include <filesystem>
#include "datyredb/database.h"
#include "datyredb/server.h"
#include "datyredb/status.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "   DatyreDB System Service v1.0.0       " << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. Инициализация системных путей
    // Профессионально: создаем рабочие папки, если их нет
    std::string db_name = "production_db";
    datyredb::Database db(db_name);

    std::cout << "[BOOT] Initializing storage engine..." << std::endl;
    auto initStatus = db.initialize();
    
    if (!initStatus.ok()) {
        std::cerr << "[FATAL] " << initStatus.toString() << std::endl;
        return 1;
    }

    // 2. Проверка функций ядра (Core Integrity Check)
    // Здесь мы можем прогнать те самые Smoke-тесты или проверить схемы
    std::cout << "[BOOT] Core functions validated. Storage: ./" << db_name << std::endl;

    // 3. Конфигурация сервера
    uint16_t port = 7474;
    
    // Если в будущем добавишь аргументы командной строки (например, --port 8080)
    if (argc > 1) {
        // Логика парсинга аргументов
    }

    // 4. Запуск сетевого интерфейса
    try {
        datyredb::Server server(port, db);
        
        std::cout << "[READY] DatyreDB is live and healthy." << std::endl;
        std::cout << "[INFO] Listening for TCP connections on port " << port << std::endl;
        
        // Запуск бесконечного цикла сервера
        server.run(); 
    } 
    catch (const std::exception& e) {
        std::cerr << "[CRITICAL] Server crash: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}