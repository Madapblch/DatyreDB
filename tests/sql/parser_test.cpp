#include <gtest/gtest.h>
#include "datyredb/sql/lexer.h"
#include "datyredb/sql/parser.h"

using namespace datyredb::sql;

TEST(ParserTest, SimpleSelect) {
    std::string query = "SELECT id FROM users";
    
    Lexer lexer(query);
    auto tokens = lexer.tokenize();
    
    Parser parser(std::move(tokens));
    auto stmt = parser.parse();
    
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->get_type(), Statement::Type::SELECT);
    
    auto* select = dynamic_cast<SelectStatement*>(stmt.get());
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->table_name_, "users");
    EXPECT_EQ(select->columns_.size(), 1);
}

TEST(ParserTest, SelectWithWhere) {
    std::string query = "SELECT name FROM users WHERE id = 1";
    
    Lexer lexer(query);
    Parser parser(lexer.tokenize());
    
    auto stmt = parser.parse();
    auto* select = dynamic_cast<SelectStatement*>(stmt.get());
    
    ASSERT_NE(select, nullptr);
    EXPECT_NE(select->where_clause_, nullptr);
}

TEST(ParserTest, Insert) {
    std::string query = "INSERT INTO users VALUES (1, 'Alice')";
    
    Lexer lexer(query);
    Parser parser(lexer.tokenize());
    
    auto stmt = parser.parse();
    
    EXPECT_EQ(stmt->get_type(), Statement::Type::INSERT);
    
    auto* insert = dynamic_cast<InsertStatement*>(stmt.get());
    ASSERT_NE(insert, nullptr);
    EXPECT_EQ(insert->table_name_, "users");
    EXPECT_EQ(insert->values_.size(), 2);
}
