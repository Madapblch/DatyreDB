#pragma once

#include "datyredb/sql/lexer.h"
#include "datyredb/sql/ast.h"
#include <memory>
#include <stdexcept>

namespace datyredb::sql {

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& message)
        : std::runtime_error(message) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    
    // Основной метод - парсинг SQL запроса
    std::unique_ptr<Statement> parse();
    
private:
    // Парсинг конкретных типов statements
    std::unique_ptr<SelectStatement> parse_select();
    std::unique_ptr<InsertStatement> parse_insert();
    std::unique_ptr<UpdateStatement> parse_update();
    std::unique_ptr<DeleteStatement> parse_delete();
    std::unique_ptr<CreateTableStatement> parse_create_table();
    
    // Парсинг выражений (рекурсивный спуск)
    std::unique_ptr<Expression> parse_expression();
    std::unique_ptr<Expression> parse_or_expression();
    std::unique_ptr<Expression> parse_and_expression();
    std::unique_ptr<Expression> parse_comparison();
    std::unique_ptr<Expression> parse_term();
    std::unique_ptr<Expression> parse_factor();
    std::unique_ptr<Expression> parse_primary();
    
    // Утилиты
    Token current() const;
    Token peek() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);
    bool is_at_end() const;
    
    ParseError error(const std::string& message);
    
    std::vector<Token> tokens_;
    size_t current_{0};
};

} // namespace datyredb::sql
