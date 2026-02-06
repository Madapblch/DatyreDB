#ifndef DATYREDB_ICOMMAND_H
#define DATYREDB_ICOMMAND_H

#include <nlohmann/json.hpp>
#include "datyredb/database.h"
#include "datyredb/status.h"

namespace datyredb {

/**
 * @brief Базовый интерфейс для всех команд СУБД.
 * Это "Сердце" расширяемости: серверу не важно, какая это команда,
 * он просто вызывает метод execute().
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * @param db Ссылка на ядро базы данных
     * @param args JSON-аргументы, пришедшие из сети
     * @return Пара: Статус выполнения и JSON-ответ для клиента
     */
    virtual std::pair<Status, nlohmann::json> execute(Database& db, const nlohmann::json& args) = 0;
};

} // namespace datyredb

#endif