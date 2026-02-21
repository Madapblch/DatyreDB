#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <nlohmann/json.hpp>

#include "datyredb/status.hpp"

namespace datyre {

    class Row {
    public:
        Row() = default;
        explicit Row(std::vector<std::string> values);

        // Методы доступа
        const std::string& at(size_t index) const;
        size_t size() const;
        const std::vector<std::string>& values() const;
        
        nlohmann::json to_json() const;

    private:
        std::vector<std::string> values_;
    };

    class QueryResult {
    public:
        // Конструкторы
        QueryResult(); 
        explicit QueryResult(Status status);
        QueryResult(std::vector<std::string> columns, std::vector<Row> rows);

        // Move semantics
        QueryResult(const QueryResult&) = delete;
        QueryResult& operator=(const QueryResult&) = delete;
        
        QueryResult(QueryResult&&) noexcept = default;
        QueryResult& operator=(QueryResult&&) noexcept = default;

        // Fluent Interface
        QueryResult& with_message(std::string msg);

        // Геттеры
        [[nodiscard]] bool ok() const;
        [[nodiscard]] const Status& status() const;
        [[nodiscard]] const std::string& message() const;
        
        size_t row_count() const;
        const std::vector<std::string>& columns() const;
        const std::vector<Row>& rows() const;

        // Итераторы
        std::vector<Row>::const_iterator begin() const;
        std::vector<Row>::const_iterator end() const;

        // Фабричные методы (Default arguments только здесь!)
        static QueryResult Success(std::string msg = "OK");
        static QueryResult Error(Status status);
        static QueryResult FromData(std::vector<std::string> cols, std::vector<std::vector<std::string>> raw_rows);

    private:
        Status status_;
        std::string message_;
        std::vector<std::string> columns_;
        std::vector<Row> rows_;
    };

} // namespace datyre
