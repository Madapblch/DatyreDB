#include "sql/parser.hpp"
#include <stdexcept>

namespace datyre {
namespace sql {

    Parser::Parser(std::unique_ptr<Lexer> lexer) : lexer_(std::move(lexer)) {
        // Заполняем буфер токенов
        next_token();
        next_token();
    }

    void Parser::next_token() {
        current_token_ = peek_token_;
        peek_token_ = lexer_->next_token();
    }

    std::unique_ptr<Statement> Parser::parse_statement() {
        switch (current_token_.type) {
            case TokenType::CREATE: return parse_create_table();
            case TokenType::INSERT: return parse_insert();
            case TokenType::SELECT: return parse_select();
            default: return nullptr;
        }
    }

    std::unique_ptr<CreateStatement> Parser::parse_create_table() {
        auto stmt = std::make_unique<CreateStatement>();
        
        if (!expect_peek(TokenType::TABLE)) return nullptr; // Skip TABLE
        if (!expect_peek(TokenType::IDENTIFIER)) return nullptr; // Table Name
        stmt->table_name = current_token_.literal;

        if (!expect_peek(TokenType::LPAREN)) return nullptr;

        // Parse columns (simplified: just names)
        while (peek_token_.type != TokenType::RPAREN && peek_token_.type != TokenType::END_OF_FILE) {
            next_token();
            if (current_token_.type == TokenType::IDENTIFIER) {
                stmt->columns.push_back(current_token_.literal);
            }
            if (peek_token_.type == TokenType::COMMA) next_token();
        }
        
        if (!expect_peek(TokenType::RPAREN)) return nullptr;
        return stmt;
    }

    std::unique_ptr<InsertStatement> Parser::parse_insert() {
        auto stmt = std::make_unique<InsertStatement>();

        if (!expect_peek(TokenType::INTO)) return nullptr;
        if (!expect_peek(TokenType::IDENTIFIER)) return nullptr;
        stmt->table_name = current_token_.literal;

        if (!expect_peek(TokenType::VALUES)) return nullptr;
        if (!expect_peek(TokenType::LPAREN)) return nullptr;

        while (peek_token_.type != TokenType::RPAREN && peek_token_.type != TokenType::END_OF_FILE) {
            next_token();
            // Поддерживаем строки, числа и идентификаторы как значения
            if (current_token_.type == TokenType::STRING_LITERAL || 
                current_token_.type == TokenType::NUMBER ||
                current_token_.type == TokenType::IDENTIFIER) {
                stmt->values.push_back(current_token_.literal);
            }
            if (peek_token_.type == TokenType::COMMA) next_token();
        }

        if (!expect_peek(TokenType::RPAREN)) return nullptr;
        return stmt;
    }

    std::unique_ptr<SelectStatement> Parser::parse_select() {
        auto stmt = std::make_unique<SelectStatement>();

        // Parse columns
        while (peek_token_.type != TokenType::FROM && peek_token_.type != TokenType::END_OF_FILE) {
            next_token();
            if (current_token_.type == TokenType::ASTERISK || current_token_.type == TokenType::IDENTIFIER) {
                stmt->columns.push_back(current_token_.literal);
            }
            if (peek_token_.type == TokenType::COMMA) next_token();
        }

        if (!expect_peek(TokenType::FROM)) return nullptr;
        if (!expect_peek(TokenType::IDENTIFIER)) return nullptr;
        stmt->table_name = current_token_.literal;

        return stmt;
    }

    bool Parser::expect_peek(TokenType type) {
        if (peek_token_.type == type) {
            next_token();
            return true;
        }
        // В реальном проекте здесь нужно бросать исключение или писать в лог ошибок
        return false;
    }

}
}
