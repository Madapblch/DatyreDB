#pragma once

#include <utility>
#include <nlohmann/json.hpp>
#include "datyredb/status.hpp"

namespace datyre {

    // Forward declaration (чтобы не создавать циклические зависимости)
    class Database;

namespace commands {

    class ICommand {
    public:
        virtual ~ICommand() = default;

        // Профессиональный подход: Команда возвращает Статус и Результат (JSON),
        // принимая Базу Данных и Аргументы.
        virtual std::pair<Status, nlohmann::json> execute(Database& db, const nlohmann::json& args) = 0;
    };

} // namespace commands
} // namespace datyre
