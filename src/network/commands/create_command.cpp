#include "datyredb/commands/icommand.hpp"
#include "core/database.hpp"
#include <utility>
#include <string>

namespace datyre {
namespace network {

    class CreateTableCommand : public datyre::commands::ICommand {
    public:
        // Реализация виртуального метода. Сигнатура должна быть ИДЕНТИЧНОЙ интерфейсу.
        std::pair<Status, nlohmann::json> execute(Database& db, const nlohmann::json& args) override {
            
            // Проверка аргументов (Defensive Programming)
            if (!args.contains("name")) {
                return {Status::InvalidArgument("Table name is required"), {}};
            }

            std::string table_name = args["name"];
            
            // Логика создания таблицы (вызов движка)
            // db.CreateTable(table_name, ...); 
            
            // Заглушка для компиляции:
            (void)db; 
            
            return {Status::OK(), {{"message", "Table created successfully"}}};
        }
    };

} // namespace network
} // namespace datyre
