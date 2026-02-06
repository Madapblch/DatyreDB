#include <gtest/gtest.h>
#include <filesystem>
#include "datyredb/core/database.h"

namespace fs = std::filesystem;

class DatabaseFixture : public ::testing::Test {
protected:
    // Выполняется перед КАЖДЫМ тестом
    void SetUp() override {
        test_path = "tmp_test_db";
        if(fs::exists(test_path)) fs::remove_all(test_path);
    }

    // Выполняется после КАЖДОГО теста
    void TearDown() override {
        fs::remove_all(test_path);
    }

    std::string test_path;
};

TEST_F(DatabaseFixture, FullCycleTest) {
    datyredb::Database db(test_path);
    
    // Проверка Status.h
    auto init_status = db.initialize();
    ASSERT_TRUE(init_status.ok()) << init_status.toString();
    
    // Проверка логики создания таблиц
    auto table_status = db.createTable("users", {{"id", "int"}});
    EXPECT_TRUE(table_status.ok());
    EXPECT_TRUE(fs::exists(fs::path(test_path) / "users.json"));
}