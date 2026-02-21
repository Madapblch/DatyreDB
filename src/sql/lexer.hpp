#pragma once

#include <string>
#include <vector>
#include <iostream>

namespace datyre {
namespace sql {

    enum class TokenType {
        // Keywords
        SELECT, FROM, WHERE, INSERT, INTO, VALUES, CREATE, TABLE,
        // Symbols
        ASTERISK, COMMA, LPAREN, RPAREN, EQUALS, SEMICOLON,
        // Data
        IDENTIFIER, STRING_LITERAL, NUMBER,
        // Control
        END_OF_FILE, ILLEGAL
    };

    struct Token {
        TokenType type;
        std::string literal;
        int line;
        int column;
    };

    class Lexer {
    public:
        explicit Lexer(std::string input);

        // Возвращает следующий токен и сдвигает позицию
        Token next_token();

        // Смотрит следующий токен без сдвига
        Token peek_token();

    private:
        std::string input_;
        size_t position_ = 0;
        size_t read_position_ = 0;
        char ch_ = 0;
        int line_ = 1;
        int column_ = 0;

        void read_char();
        char peek_char();
        void skip_whitespace();
        std::string read_identifier();
        std::string read_string();
        std::string read_number();
        TokenType lookup_ident(const std::string& ident);
        bool is_letter(char c);
        bool is_digit(char c);
    };

} // namespace sql
} // namespace datyre
