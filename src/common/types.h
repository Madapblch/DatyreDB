#pragma once

#include <cstddef>   // size_t
#include <cstdint>   // std::uint32_t
#include <array>     // std::array
#include <string>
#include <vector>
#include <variant>

using page_id_t = std::uint32_t;

inline constexpr std::size_t PAGE_SIZE = 4096;

// Страница фиксированного размера 4 КБ
using Page = std::array<char, PAGE_SIZE>;

enum class Status {
    OK,
    ERROR,
    PAGE_NOT_FOUND,
    FILE_NOT_FOUND
};

// SQL Types
enum class DataType {
    INT,
    VARCHAR
};

using Value = std::variant<int, std::string>;

struct Column {
    std::string name;
    DataType type;
};

// WHERE clause operators (like in PostgreSQL)
enum class ComparisonOp {
    EQ,      // =
    NE,      // !=
    LT,      // <
    LE,      // <=
    GT,      // >
    GE       // >=
};

struct WhereCondition {
    std::string column_name;
    ComparisonOp op;
    Value value;
};

struct Table {
    std::string name;
    std::vector<Column> columns;
    page_id_t first_page = 0; // Starting page for data
};

struct Row {
    std::vector<Value> values;
};