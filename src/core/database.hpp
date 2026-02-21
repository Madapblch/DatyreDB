#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <shared_mutex>
#include <mutex>

// Подключаем зависимости
#include "core/table.hpp"
#include "datyredb/status.hpp"

namespace datyre {

    class Database {
    public:
        // Конструктор по умолчанию
        Database();
        
        // Деструктор
        ~Database();

        // Главный метод выполнения запросов (строка -> результат)
        std::string execute(const std::string& query);

        // Методы управления таблицами
        Status CreateTable(const std::string& name, const std::vector<std::string>& columns);
        std::shared_ptr<Table> GetTable(const std::string& name);

        // Методы сохранения (Persistence)
        void shutdown();

    private:
        // Хранилище таблиц: Имя -> Объект Таблицы
        std::map<std::string, std::shared_ptr<Table>> tables_;
        
        // Мьютекс для защиты списка таблиц (Reader-Writer Lock)
        // mutable позволяет блокировать мьютекс даже в const методах
        mutable std::shared_mutex tables_mutex_;

        // Путь к директории данных
        std::string data_dir_;

        void load_tables();
    };

} // namespace datyre
