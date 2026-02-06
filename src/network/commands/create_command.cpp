#include "datyredb/commands/icommand.h"

namespace datyredb {

class CreateTableCommand : public ICommand {
public:
    std::pair<Status, nlohmann::json> execute(Database& db, const nlohmann::json& args) override {
        // 1. Валидация протокола: есть ли имя и схема?
        if (!args.contains("name") || !args.contains("schema")) {
            return { Status::IOError("Protocol error: 'name' and 'schema' are required"), {} };
        }

        std::string tableName = args["name"];
        nlohmann::json schema = args["schema"];

        // 2. Вызов ядра базы
        Status s = db.createTable(tableName, schema);

        // 3. Формирование ответа
        nlohmann::json result;
        if (s.ok()) {
            result["message"] = "Table '" + tableName + "' created successfully";
        }

        return { s, result };
    }
};

} // namespace datyredb