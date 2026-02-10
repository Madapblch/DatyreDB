#pragma once

#include "token.h"
#include <vector>
#include <string_view>
#include <unordered_map>

namespace datyredb::sql {

class Lexer {
public:
    explicit Lexer(std::string_view source);
    
    // Основной метод - токенизация всего запроса
    std::vector<Token> tokenize();
    
    // Получить следующий токен
    Token next_token();
    
    // Проверить текущий токен без продвижения
    Token peek() const;
    
    // Есть ли еще токены
    bool has_next() const;
    
private:
    void skip_whitespace();
    void skip_comment();
    
    Token scan_identifier();
    Token scan_number();
    Token scan_string();
    Token scan_operator();
    
    char current() const;
    char peek_char() const;
    char advance();
    bool match(char expected);
    
    bool is_at_end() const;
    bool is_alpha(char c) const;
    bool is_digit(char c) const;
    bool is_alphanumeric(char c) const;
    
    std::string_view source_;
    size_t start_{0};
    size_t current_{0};
    size_t line_{1};
    size_t column_{1};
    
    // Словарь ключевых слов
    static const std::unordered_map<std::string, TokenType> keywords_;
};

} // namespace datyredb::sql
