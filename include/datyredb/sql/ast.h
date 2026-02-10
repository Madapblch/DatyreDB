#pragma once

#include <memory>
#include <string>
#include <vector>

namespace datyredb::sql {

// Базовый класс для всех AST узлов
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual std::string to_string() const = 0;
};

// Выражения
class Expression : public ASTNode {
public:
    enum class Type {
        LITERAL,
        IDENTIFIER,
        BINARY_OP,
        UNARY_OP
    };
    
    virtual Type get_type() const = 0;
};

// Литералы (числа, строки)
class LiteralExpression : public Expression {
public:
    enum class LiteralType {
        INTEGER,
        STRING,
        NULL_VALUE
    };
    
    explicit LiteralExpression(int64_t value) 
        : literal_type_(LiteralType::INTEGER), int_value_(value) {}
    
    explicit LiteralExpression(std::string value)
        : literal_type_(LiteralType::STRING), string_value_(std::move(value)) {}
    
    Type get_type() const override { return Type::LITERAL; }
    
    LiteralType literal_type_;
    int64_t int_value_{0};
    std::string string_value_;
    
    std::string to_string() const override;
};

// Идентификаторы (имена таблиц, колонок)
class IdentifierExpression : public Expression {
public:
    explicit IdentifierExpression(std::string name)
        : name_(std::move(name)) {}
    
    Type get_type() const override { return Type::IDENTIFIER; }
    
    std::string name_;
    
    std::string to_string() const override { return name_; }
};

// Бинарные операторы (=, <, >, AND, OR)
class BinaryExpression : public Expression {
public:
    enum class Operator {
        EQUAL,
        NOT_EQUAL,
        LESS,
        LESS_EQUAL,
        GREATER,
        GREATER_EQUAL,
        AND,
        OR,
        PLUS,
        MINUS,
        MULTIPLY,
        DIVIDE
    };
    
    BinaryExpression(Operator op, 
                     std::unique_ptr<Expression> left,
                     std::unique_ptr<Expression> right)
        : op_(op), left_(std::move(left)), right_(std::move(right)) {}
    
    Type get_type() const override { return Type::BINARY_OP; }
    
    Operator op_;
    std::unique_ptr<Expression> left_;
    std::unique_ptr<Expression> right_;
    
    std::string to_string() const override;
};

// SQL Statements
class Statement : public ASTNode {
public:
    enum class Type {
        SELECT,
        INSERT,
        UPDATE,
        DELETE,
        CREATE_TABLE,
        DROP_TABLE
    };
    
    virtual Type get_type() const = 0;
};

// SELECT statement
class SelectStatement : public Statement {
public:
    Type get_type() const override { return Type::SELECT; }
    
    std::vector<std::unique_ptr<Expression>> columns_;  // SELECT колонки
    std::string table_name_;                             // FROM таблица
    std::unique_ptr<Expression> where_clause_;          // WHERE условие (опционально)
    
    std::string to_string() const override;
};

// INSERT statement
class InsertStatement : public Statement {
public:
    Type get_type() const override { return Type::INSERT; }
    
    std::string table_name_;
    std::vector<std::string> column_names_;
    std::vector<std::unique_ptr<Expression>> values_;
    
    std::string to_string() const override;
};

// UPDATE statement
class UpdateStatement : public Statement {
public:
    Type get_type() const override { return Type::UPDATE; }
    
    std::string table_name_;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> assignments_;
    std::unique_ptr<Expression> where_clause_;
    
    std::string to_string() const override;
};

// DELETE statement
class DeleteStatement : public Statement {
public:
    Type get_type() const override { return Type::DELETE; }
    
    std::string table_name_;
    std::unique_ptr<Expression> where_clause_;
    
    std::string to_string() const override;
};

// CREATE TABLE statement
class CreateTableStatement : public Statement {
public:
    struct ColumnDefinition {
        std::string name;
        std::string type;  // INT, VARCHAR, etc.
        bool is_primary_key{false};
    };
    
    Type get_type() const override { return Type::CREATE_TABLE; }
    
    std::string table_name_;
    std::vector<ColumnDefinition> columns_;
    
    std::string to_string() const override;
};

} // namespace datyredb::sql
