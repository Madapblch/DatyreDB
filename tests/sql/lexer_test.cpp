#include <gtest/gtest.h>
#include "datyredb/sql/lexer.h"

using namespace datyredb::sql;

TEST(LexerTest, SimpleSelect) {
    std::string query = "SELECT id FROM users";
    Lexer lexer(query);
    
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 5); // SELECT, id, FROM, users, EOF
    EXPECT_EQ(tokens[0].type, TokenType::SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].lexeme, "id");
    EXPECT_EQ(tokens[2].type, TokenType::FROM);
    EXPECT_EQ(tokens[3].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[3].lexeme, "users");
}

TEST(LexerTest, SelectWithWhere) {
    std::string query = "SELECT name FROM users WHERE id = 1";
    Lexer lexer(query);
    
    auto tokens = lexer.tokenize();
    
    EXPECT_EQ(tokens[4].type, TokenType::WHERE);
    EXPECT_EQ(tokens[6].type, TokenType::EQUAL);
    EXPECT_EQ(tokens[7].type, TokenType::INTEGER_LITERAL);
}

TEST(LexerTest, StringLiteral) {
    std::string query = "SELECT * FROM users WHERE name = 'John'";
    Lexer lexer(query);
    
    auto tokens = lexer.tokenize();
    
    bool found_string = false;
    for (const auto& token : tokens) {
        if (token.type == TokenType::STRING_LITERAL) {
            EXPECT_EQ(std::get<std::string>(token.value), "John");
            found_string = true;
        }
    }
    EXPECT_TRUE(found_string);
}

TEST(LexerTest, Insert) {
    std::string query = "INSERT INTO users VALUES (1, 'Alice')";
    Lexer lexer(query);
    
    auto tokens = lexer.tokenize();
    
    EXPECT_EQ(tokens[0].type, TokenType::INSERT);
    EXPECT_EQ(tokens[1].type, TokenType::INTO);
    EXPECT_EQ(tokens[3].type, TokenType::VALUES);
}
