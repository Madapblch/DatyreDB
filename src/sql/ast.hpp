#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace datyre {
namespace sql {

    enum class StatementType {
        CREATE_TABLE,
        INSERT,
        SELECT,
        UNKNOWN
    };

    // Базовый класс для всех инструкций
    class Statement {
    public:
        virtual ~Statement() = default;
        virtual StatementType type() const = 0;
        virtual std::string to_string() const = 0;
    };

    // CREATE TABLE users (id, name)
    class CreateStatement : public Statement {
    public:
        std::string table_name;
        std::vector<std::string> columns;

        StatementType type() const override { return StatementType::CREATE_TABLE; }
        std::string to_string() const override;
    };

    // INSERT INTO users VALUES (1, "admin")
    class InsertStatement : public Statement {
    public:
        std::string table_name;
        std::vector<std::string> values;

        StatementType type() const override { return StatementType::INSERT; }
        std::string to_string() const override;
    };

    // SELECT * FROM users
    class SelectStatement : public Statement {
    public:
        std::string table_name;
        std::vector<std::string> columns; // "*" или список

        StatementType type() const override { return StatementType::SELECT; }
        std::string to_string() const override;
    };

} // namespace sql
} // namespace datyre
