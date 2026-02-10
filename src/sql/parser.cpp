#include "datyredb/sql/parser.h"

namespace datyredb::sql {

Parser::Parser(std::vector<Token> tokens) 
    : tokens_(std::move(tokens)) {}

std::unique_ptr<Statement> Parser::parse() {
    Token first = current();
    
    switch (first.type) {
        case TokenType::SELECT:
            return parse_select();
        case TokenType::INSERT:
            return parse_insert();
        case TokenType::UPDATE:
            return parse_update();
        case TokenType::DELETE:
            return parse_delete();
        case TokenType::CREATE:
            return parse_create_table();
        default:
            throw error("Expected SQL statement (SELECT, INSERT, UPDATE, DELETE, CREATE)");
    }
}

std::unique_ptr<SelectStatement> Parser::parse_select() {
    auto stmt = std::make_unique<SelectStatement>();
    
    // Consume SELECT keyword
    consume(TokenType::SELECT, "Expected SELECT");
    
    // Parse columns (SELECT col1, col2, ...)
    if (match(TokenType::STAR)) {
        // SELECT * - все колонки
        stmt->columns_.push_back(
            std::make_unique<IdentifierExpression>("*")
        );
    } else {
        do {
            stmt->columns_.push_back(parse_primary());
        } while (match(TokenType::COMMA));
    }
    
    // Parse FROM clause
    consume(TokenType::FROM, "Expected FROM after column list");
    Token table_name = consume(TokenType::IDENTIFIER, "Expected table name");
    stmt->table_name_ = table_name.lexeme;
    
    // Parse optional WHERE clause
    if (match(TokenType::WHERE)) {
        stmt->where_clause_ = parse_expression();
    }
    
    // Consume optional semicolon
    match(TokenType::SEMICOLON);
    
    return stmt;
}

std::unique_ptr<InsertStatement> Parser::parse_insert() {
    auto stmt = std::make_unique<InsertStatement>();
    
    consume(TokenType::INSERT, "Expected INSERT");
    consume(TokenType::INTO, "Expected INTO");
    
    Token table_name = consume(TokenType::IDENTIFIER, "Expected table name");
    stmt->table_name_ = table_name.lexeme;
    
    // Optional column list: INSERT INTO table (col1, col2)
    if (match(TokenType::LEFT_PAREN)) {
        do {
            Token col = consume(TokenType::IDENTIFIER, "Expected column name");
            stmt->column_names_.push_back(col.lexeme);
        } while (match(TokenType::COMMA));
        
        consume(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    }
    
    // VALUES clause
    consume(TokenType::VALUES, "Expected VALUES");
    consume(TokenType::LEFT_PAREN, "Expected '(' after VALUES");
    
    do {
        stmt->values_.push_back(parse_primary());
    } while (match(TokenType::COMMA));
    
    consume(TokenType::RIGHT_PAREN, "Expected ')' after values");
    match(TokenType::SEMICOLON);
    
    return stmt;
}

std::unique_ptr<Expression> Parser::parse_expression() {
    return parse_or_expression();
}

std::unique_ptr<Expression> Parser::parse_or_expression() {
    auto expr = parse_and_expression();
    
    while (match(TokenType::OR)) {
        auto right = parse_and_expression();
        expr = std::make_unique<BinaryExpression>(
            BinaryExpression::Operator::OR,
            std::move(expr),
            std::move(right)
        );
    }
    
    return expr;
}

std::unique_ptr<Expression> Parser::parse_and_expression() {
    auto expr = parse_comparison();
    
    while (match(TokenType::AND)) {
        auto right = parse_comparison();
        expr = std::make_unique<BinaryExpression>(
            BinaryExpression::Operator::AND,
            std::move(expr),
            std::move(right)
        );
    }
    
    return expr;
}

std::unique_ptr<Expression> Parser::parse_comparison() {
    auto expr = parse_term();
    
    while (true) {
        BinaryExpression::Operator op;
        
        if (match(TokenType::EQUAL)) {
            op = BinaryExpression::Operator::EQUAL;
        } else if (match(TokenType::NOT_EQUAL)) {
            op = BinaryExpression::Operator::NOT_EQUAL;
        } else if (match(TokenType::LESS)) {
            op = BinaryExpression::Operator::LESS;
        } else if (match(TokenType::LESS_EQUAL)) {
            op = BinaryExpression::Operator::LESS_EQUAL;
        } else if (match(TokenType::GREATER)) {
            op = BinaryExpression::Operator::GREATER;
        } else if (match(TokenType::GREATER_EQUAL)) {
            op = BinaryExpression::Operator::GREATER_EQUAL;
        } else {
            break;
        }
        
        auto right = parse_term();
        expr = std::make_unique<BinaryExpression>(
            op, std::move(expr), std::move(right)
        );
    }
    
    return expr;
}

std::unique_ptr<Expression> Parser::parse_term() {
    auto expr = parse_factor();
    
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        TokenType op_type = tokens_[current_ - 1].type;
        auto op = (op_type == TokenType::PLUS) 
            ? BinaryExpression::Operator::PLUS
            : BinaryExpression::Operator::MINUS;
        
        auto right = parse_factor();
        expr = std::make_unique<BinaryExpression>(
            op, std::move(expr), std::move(right)
        );
    }
    
    return expr;
}

std::unique_ptr<Expression> Parser::parse_factor() {
    auto expr = parse_primary();
    
    while (match(TokenType::STAR) || match(TokenType::SLASH)) {
        TokenType op_type = tokens_[current_ - 1].type;
        auto op = (op_type == TokenType::STAR)
            ? BinaryExpression::Operator::MULTIPLY
            : BinaryExpression::Operator::DIVIDE;
        
        auto right = parse_primary();
        expr = std::make_unique<BinaryExpression>(
            op, std::move(expr), std::move(right)
        );
    }
    
    return expr;
}

std::unique_ptr<Expression> Parser::parse_primary() {
    // Integer literal
    if (check(TokenType::INTEGER_LITERAL)) {
        Token token = advance();
        return std::make_unique<LiteralExpression>(
            std::get<int64_t>(token.value)
        );
    }
    
    // String literal
    if (check(TokenType::STRING_LITERAL)) {
        Token token = advance();
        return std::make_unique<LiteralExpression>(
            std::get<std::string>(token.value)
        );
    }
    
    // Identifier
    if (check(TokenType::IDENTIFIER)) {
        Token token = advance();
        return std::make_unique<IdentifierExpression>(token.lexeme);
    }
    
    // Parenthesized expression
    if (match(TokenType::LEFT_PAREN)) {
        auto expr = parse_expression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after expression");
        return expr;
    }
    
    throw error("Expected expression");
}

// Utility methods
Token Parser::current() const {
    return tokens_[current_];
}

Token Parser::peek() const {
    if (current_ + 1 < tokens_.size()) {
        return tokens_[current_ + 1];
    }
    return tokens_.back();
}

Token Parser::advance() {
    if (!is_at_end()) current_++;
    return tokens_[current_ - 1];
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    throw error(message);
}

bool Parser::is_at_end() const {
    return current().type == TokenType::END_OF_FILE;
}

ParseError Parser::error(const std::string& message) {
    Token token = current();
    return ParseError(
        "Parse error at line " + std::to_string(token.line) +
        ", column " + std::to_string(token.column) +
        ": " + message
    );
}

// Добавь эти методы в конец файла src/sql/parser.cpp

std::unique_ptr<UpdateStatement> Parser::parse_update() {
    auto stmt = std::make_unique<UpdateStatement>();
    
    // Consume UPDATE keyword
    consume(TokenType::UPDATE, "Expected UPDATE");
    
    // Parse table name
    Token table_name = consume(TokenType::IDENTIFIER, "Expected table name");
    stmt->table_name_ = table_name.lexeme;
    
    // Parse SET clause
    consume(TokenType::SET, "Expected SET");
    
    // Parse assignments (col1 = val1, col2 = val2, ...)
    do {
        Token column = consume(TokenType::IDENTIFIER, "Expected column name");
        consume(TokenType::EQUAL, "Expected '=' after column name");
        auto value = parse_primary();
        
        stmt->assignments_.emplace_back(column.lexeme, std::move(value));
    } while (match(TokenType::COMMA));
    
    // Parse optional WHERE clause
    if (match(TokenType::WHERE)) {
        stmt->where_clause_ = parse_expression();
    }
    
    // Consume optional semicolon
    match(TokenType::SEMICOLON);
    
    return stmt;
}

std::unique_ptr<DeleteStatement> Parser::parse_delete() {
    auto stmt = std::make_unique<DeleteStatement>();
    
    // Consume DELETE keyword
    consume(TokenType::DELETE, "Expected DELETE");
    
    // Consume FROM keyword
    consume(TokenType::FROM, "Expected FROM");
    
    // Parse table name
    Token table_name = consume(TokenType::IDENTIFIER, "Expected table name");
    stmt->table_name_ = table_name.lexeme;
    
    // Parse optional WHERE clause
    if (match(TokenType::WHERE)) {
        stmt->where_clause_ = parse_expression();
    }
    
    // Consume optional semicolon
    match(TokenType::SEMICOLON);
    
    return stmt;
}

std::unique_ptr<CreateTableStatement> Parser::parse_create_table() {
    auto stmt = std::make_unique<CreateTableStatement>();
    
    // Consume CREATE TABLE keywords
    consume(TokenType::CREATE, "Expected CREATE");
    consume(TokenType::TABLE, "Expected TABLE after CREATE");
    
    // Parse table name
    Token table_name = consume(TokenType::IDENTIFIER, "Expected table name");
    stmt->table_name_ = table_name.lexeme;
    
    // Parse column definitions
    consume(TokenType::LEFT_PAREN, "Expected '(' after table name");
    
    do {
        CreateTableStatement::ColumnDefinition col;
        
        // Column name
        Token col_name = consume(TokenType::IDENTIFIER, "Expected column name");
        col.name = col_name.lexeme;
        
        // Column type (simplified - just read as identifier for now)
        Token col_type = consume(TokenType::IDENTIFIER, "Expected column type");
        col.type = col_type.lexeme;
        
        // Check for PRIMARY KEY
        if (check(TokenType::IDENTIFIER)) {
            Token next = current();
            if (next.lexeme == "PRIMARY" || next.lexeme == "primary") {
                advance(); // consume PRIMARY
                Token key = current();
                if (key.type == TokenType::IDENTIFIER && 
                    (key.lexeme == "KEY" || key.lexeme == "key")) {
                    advance(); // consume KEY
                    col.is_primary_key = true;
                }
            }
        }
        
        stmt->columns_.push_back(col);
        
    } while (match(TokenType::COMMA));
    
    consume(TokenType::RIGHT_PAREN, "Expected ')' after column definitions");
    
    // Consume optional semicolon
    match(TokenType::SEMICOLON);
    
    return stmt;
}

} // namespace datyredb::sql
