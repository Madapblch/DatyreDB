#include "sql/lexer.hpp"
#include <algorithm>
#include <map>

namespace datyre {
namespace sql {

    Lexer::Lexer(std::string input) : input_(std::move(input)) {
        read_char();
    }

    void Lexer::read_char() {
        if (read_position_ >= input_.length()) {
            ch_ = 0;
        } else {
            ch_ = input_[read_position_];
        }
        position_ = read_position_;
        read_position_++;
        column_++;
    }

    char Lexer::peek_char() {
        if (read_position_ >= input_.length()) {
            return 0;
        }
        return input_[read_position_];
    }

    void Lexer::skip_whitespace() {
        while (ch_ == ' ' || ch_ == '\t' || ch_ == '\n' || ch_ == '\r') {
            if (ch_ == '\n') {
                line_++;
                column_ = 0;
            }
            read_char();
        }
    }

    Token Lexer::next_token() {
        skip_whitespace();
        Token tok;
        tok.line = line_;
        tok.column = column_;

        switch (ch_) {
            case '*': tok = {TokenType::ASTERISK, std::string(1, ch_), line_, column_}; break;
            case ',': tok = {TokenType::COMMA, std::string(1, ch_), line_, column_}; break;
            case ';': tok = {TokenType::SEMICOLON, std::string(1, ch_), line_, column_}; break;
            case '(': tok = {TokenType::LPAREN, std::string(1, ch_), line_, column_}; break;
            case ')': tok = {TokenType::RPAREN, std::string(1, ch_), line_, column_}; break;
            case '=': tok = {TokenType::EQUALS, std::string(1, ch_), line_, column_}; break;
            case 0:   tok = {TokenType::END_OF_FILE, "", line_, column_}; break;
            case '\'':
            case '"':
                tok.type = TokenType::STRING_LITERAL;
                tok.literal = read_string();
                return tok; // read_string already advanced char
            default:
                if (is_letter(ch_)) {
                    tok.literal = read_identifier();
                    tok.type = lookup_ident(tok.literal);
                    return tok;
                } else if (is_digit(ch_)) {
                    tok.type = TokenType::NUMBER;
                    tok.literal = read_number();
                    return tok;
                } else {
                    tok = {TokenType::ILLEGAL, std::string(1, ch_), line_, column_};
                }
        }
        read_char();
        return tok;
    }

    std::string Lexer::read_string() {
        char quote = ch_;
        read_char(); // skip opening quote
        size_t start = position_;
        while (ch_ != quote && ch_ != 0) {
            read_char();
        }
        std::string str = input_.substr(start, position_ - start);
        if (ch_ == quote) read_char(); // skip closing quote
        return str;
    }

    std::string Lexer::read_identifier() {
        size_t start = position_;
        while (is_letter(ch_) || is_digit(ch_) || ch_ == '_') {
            read_char();
        }
        return input_.substr(start, position_ - start);
    }
    
    std::string Lexer::read_number() {
        size_t start = position_;
        while (is_digit(ch_)) {
            read_char();
        }
        return input_.substr(start, position_ - start);
    }

    TokenType Lexer::lookup_ident(const std::string& ident) {
        static const std::map<std::string, TokenType> keywords = {
            {"SELECT", TokenType::SELECT}, {"FROM", TokenType::FROM},
            {"WHERE", TokenType::WHERE}, {"INSERT", TokenType::INSERT},
            {"INTO", TokenType::INTO}, {"VALUES", TokenType::VALUES},
            {"CREATE", TokenType::CREATE}, {"TABLE", TokenType::TABLE}
        };
        
        std::string upper = ident;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        auto it = keywords.find(upper);
        if (it != keywords.end()) return it->second;
        return TokenType::IDENTIFIER;
    }

    bool Lexer::is_letter(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }
    bool Lexer::is_digit(char c) {
        return c >= '0' && c <= '9';
    }

}
}
