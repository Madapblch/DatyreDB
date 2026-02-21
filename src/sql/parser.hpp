#pragma once

#include "sql/lexer.hpp"
#include "sql/ast.hpp"
#include <memory>
#include <vector>

namespace datyre {
namespace sql {

    class Parser {
    public:
        explicit Parser(std::unique_ptr<Lexer> lexer);
        
        // Главный метод: парсит запрос и возвращает AST
        std::unique_ptr<Statement> parse_statement();

    private:
        std::unique_ptr<Lexer> lexer_;
        Token current_token_;
        Token peek_token_;

        void next_token();
        bool expect_peek(TokenType type);
        
        // Методы для каждого типа инструкций (Recursive Descent)
        std::unique_ptr<CreateStatement> parse_create_table();
        std::unique_ptr<InsertStatement> parse_insert();
        std::unique_ptr<SelectStatement> parse_select();
    };

} // namespace sql
} // namespace datyre
