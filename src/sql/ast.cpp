#include "sql/ast.hpp"
#include <sstream>

namespace datyre {
namespace sql {

    std::string CreateStatement::to_string() const {
        std::stringstream ss;
        ss << "CREATE TABLE " << table_name << " (";
        for (size_t i = 0; i < columns.size(); ++i) {
            ss << columns[i] << (i < columns.size() - 1 ? ", " : "");
        }
        ss << ")";
        return ss.str();
    }

    std::string InsertStatement::to_string() const {
        std::stringstream ss;
        ss << "INSERT INTO " << table_name << " VALUES (";
        for (size_t i = 0; i < values.size(); ++i) {
            ss << values[i] << (i < values.size() - 1 ? ", " : "");
        }
        ss << ")";
        return ss.str();
    }

    std::string SelectStatement::to_string() const {
        std::stringstream ss;
        ss << "SELECT ";
        if (columns.empty()) {
            ss << "*";
        } else {
            for (size_t i = 0; i < columns.size(); ++i) {
                ss << columns[i] << (i < columns.size() - 1 ? ", " : "");
            }
        }
        ss << " FROM " << table_name;
        return ss.str();
    }

} // namespace sql
} // namespace datyre
