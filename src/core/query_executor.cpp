#include <string>

// ВАЖНО: Подключаем определение типа возвращаемого значения
#include "core/query_result.hpp"

namespace datyre {

    // Forward declaration: "Класс Database существует, не спрашивай детали сейчас"
    class Database;

    class QueryExecutor {
    public:
        // Конструктор принимает ссылку, поэтому Forward Declaration достаточно
        explicit QueryExecutor(Database& db);

        QueryResult execute(const std::string& sql);

    private:
        Database& db_;

        QueryResult execute_select(const std::string& sql);
        QueryResult execute_insert(const std::string& sql);
        QueryResult execute_create_table(const std::string& sql);
        QueryResult execute_show_tables();
    };

} // namespace datyre
