#pragma once

#include <string>
#include <variant>

namespace datyredb::sql {

enum class TokenType {
    // Keywords
    SELECT,
    FROM,
    WHERE,
    INSERT,
    INTO,
    VALUES,
    UPDATE,
    SET,
    DELETE,
    CREATE,
    TABLE,
    DROP,
    INDEX,
    AND,
    OR,
    NOT,
    
    // Operators
    EQUAL,           // =
    NOT_EQUAL,       // != or <>
    LESS,            // <
    LESS_EQUAL,      // <=
    GREATER,         // >
    GREATER_EQUAL,   // >=
    PLUS,            // +
    MINUS,           // -
    STAR,            // *
    SLASH,           // /
    
    // Delimiters
    LEFT_PAREN,      // (
    RIGHT_PAREN,     // )
    COMMA,           // ,
    SEMICOLON,       // ;
    DOT,             // .
    
    // Literals
    IDENTIFIER,      // table names, column names
    INTEGER_LITERAL,
    STRING_LITERAL,
    
    // Special
    END_OF_FILE,
    INVALID
};

struct Token {
    TokenType type;
    std::string lexeme;  // Исходный текст токена
    std::variant<int64_t, std::string> value;  // Значение для литералов
    size_t line;
    size_t column;
    
    Token(TokenType t, std::string lex, size_t l, size_t c)
        : type(t), lexeme(std::move(lex)), line(l), column(c) {}
    
    bool is_keyword() const {
        return type >= TokenType::SELECT && type <= TokenType::NOT;
    }
    
    bool is_operator() const {
        return type >= TokenType::EQUAL && type <= TokenType::SLASH;
    }
    
    std::string to_string() const;
};

} // namespace datyredb::sql
