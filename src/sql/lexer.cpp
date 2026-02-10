#include "datyredb/sql/lexer.h"
#include <cctype>
#include <stdexcept>

namespace datyredb::sql {

// Инициализация словаря ключевых слов
const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"SELECT", TokenType::SELECT},
    {"FROM", TokenType::FROM},
    {"WHERE", TokenType::WHERE},
    {"INSERT", TokenType::INSERT},
    {"INTO", TokenType::INTO},
    {"VALUES", TokenType::VALUES},
    {"UPDATE", TokenType::UPDATE},
    {"SET", TokenType::SET},
    {"DELETE", TokenType::DELETE},
    {"CREATE", TokenType::CREATE},
    {"TABLE", TokenType::TABLE},
    {"DROP", TokenType::DROP},
    {"INDEX", TokenType::INDEX},
    {"AND", TokenType::AND},
    {"OR", TokenType::OR},
    {"NOT", TokenType::NOT}
};

Lexer::Lexer(std::string_view source) : source_(source) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!is_at_end()) {
        skip_whitespace();
        if (is_at_end()) break;
        
        start_ = current_;
        Token token = next_token();
        
        if (token.type != TokenType::INVALID) {
            tokens.push_back(std::move(token));
        }
    }
    
    tokens.emplace_back(TokenType::END_OF_FILE, "", line_, column_);
    return tokens;
}

Token Lexer::next_token() {
    skip_whitespace();
    
    if (is_at_end()) {
        return Token(TokenType::END_OF_FILE, "", line_, column_);
    }
    
    start_ = current_;
    char c = advance();
    
    // Идентификаторы и ключевые слова
    if (is_alpha(c)) {
        return scan_identifier();
    }
    
    // Числа
    if (is_digit(c)) {
        return scan_number();
    }
    
    // Строковые литералы
    if (c == '\'' || c == '"') {
        return scan_string();
    }
    
    // Операторы и разделители
    switch (c) {
        case '(': return Token(TokenType::LEFT_PAREN, "(", line_, column_ - 1);
        case ')': return Token(TokenType::RIGHT_PAREN, ")", line_, column_ - 1);
        case ',': return Token(TokenType::COMMA, ",", line_, column_ - 1);
        case ';': return Token(TokenType::SEMICOLON, ";", line_, column_ - 1);
        case '.': return Token(TokenType::DOT, ".", line_, column_ - 1);
        case '+': return Token(TokenType::PLUS, "+", line_, column_ - 1);
        case '-': {
            // Проверяем комментарии --
            if (match('-')) {
                skip_comment();
                return next_token();
            }
            return Token(TokenType::MINUS, "-", line_, column_ - 1);
        }
        case '*': return Token(TokenType::STAR, "*", line_, column_ - 1);
        case '/': return Token(TokenType::SLASH, "/", line_, column_ - 1);
        case '=': return Token(TokenType::EQUAL, "=", line_, column_ - 1);
        case '<': {
            if (match('=')) {
                return Token(TokenType::LESS_EQUAL, "<=", line_, column_ - 2);
            } else if (match('>')) {
                return Token(TokenType::NOT_EQUAL, "<>", line_, column_ - 2);
            }
            return Token(TokenType::LESS, "<", line_, column_ - 1);
        }
        case '>': {
            if (match('=')) {
                return Token(TokenType::GREATER_EQUAL, ">=", line_, column_ - 2);
            }
            return Token(TokenType::GREATER, ">", line_, column_ - 1);
        }
        case '!': {
            if (match('=')) {
                return Token(TokenType::NOT_EQUAL, "!=", line_, column_ - 2);
            }
            break;
        }
    }
    
    return Token(TokenType::INVALID, std::string(1, c), line_, column_ - 1);
}

Token Lexer::scan_identifier() {
    while (is_alphanumeric(current()) || current() == '_') {
        advance();
    }
    
    std::string text(source_.substr(start_, current_ - start_));
    
    // Преобразуем в верхний регистр для поиска ключевого слова
    std::string upper_text = text;
    for (char& c : upper_text) {
        c = std::toupper(c);
    }
    
    auto it = keywords_.find(upper_text);
    if (it != keywords_.end()) {
        return Token(it->second, text, line_, column_ - text.length());
    }
    
    return Token(TokenType::IDENTIFIER, text, line_, column_ - text.length());
}

Token Lexer::scan_number() {
    while (is_digit(current())) {
        advance();
    }
    
    std::string text(source_.substr(start_, current_ - start_));
    int64_t value = std::stoll(text);
    
    Token token(TokenType::INTEGER_LITERAL, text, line_, column_ - text.length());
    token.value = value;
    return token;
}

Token Lexer::scan_string() {
    char quote = source_[start_];
    
    // Пропускаем открывающую кавычку
    while (!is_at_end() && current() != quote) {
        if (current() == '\n') {
            line_++;
            column_ = 0;
        }
        advance();
    }
    
    if (is_at_end()) {
        throw std::runtime_error("Unterminated string literal");
    }
    
    // Пропускаем закрывающую кавычку
    advance();
    
    // Извлекаем строку без кавычек
    std::string value(source_.substr(start_ + 1, current_ - start_ - 2));
    std::string lexeme(source_.substr(start_, current_ - start_));
    
    Token token(TokenType::STRING_LITERAL, lexeme, line_, column_ - lexeme.length());
    token.value = value;
    return token;
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        char c = current();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                line_++;
                column_ = 0;
                advance();
                break;
            default:
                return;
        }
    }
}

void Lexer::skip_comment() {
    // Пропускаем до конца строки
    while (!is_at_end() && current() != '\n') {
        advance();
    }
}

char Lexer::current() const {
    if (is_at_end()) return '\0';
    return source_[current_];
}

char Lexer::peek_char() const {
    if (current_ + 1 >= source_.length()) return '\0';
    return source_[current_ + 1];
}

char Lexer::advance() {
    column_++;
    return source_[current_++];
}

bool Lexer::match(char expected) {
    if (is_at_end()) return false;
    if (current() != expected) return false;
    
    advance();
    return true;
}

bool Lexer::is_at_end() const {
    return current_ >= source_.length();
}

bool Lexer::is_alpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::is_digit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::is_alphanumeric(char c) const {
    return is_alpha(c) || is_digit(c);
}

std::string Token::to_string() const {
    return "Token(" + std::to_string(static_cast<int>(type)) + 
           ", '" + lexeme + "')";
}

} // namespace datyredb::sql
