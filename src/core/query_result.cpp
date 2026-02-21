#include "core/query_result.hpp"
#include <stdexcept>

namespace datyre {

    // --- Row Implementation ---

    Row::Row(std::vector<std::string> values) : values_(std::move(values)) {}

    const std::string& Row::at(size_t index) const {
        if (index >= values_.size()) {
            throw std::out_of_range("Row index out of range");
        }
        return values_[index];
    }

    size_t Row::size() const {
        return values_.size();
    }

    const std::vector<std::string>& Row::values() const {
        return values_;
    }

    nlohmann::json Row::to_json() const {
        return values_;
    }

    // --- QueryResult Implementation ---

    QueryResult::QueryResult() : status_(Status::OK()) {}

    QueryResult::QueryResult(Status status) : status_(std::move(status)) {}

    QueryResult::QueryResult(std::vector<std::string> columns, std::vector<Row> rows)
        : status_(Status::OK()), columns_(std::move(columns)), rows_(std::move(rows)) {}

    QueryResult& QueryResult::with_message(std::string msg) {
        message_ = std::move(msg);
        return *this;
    }

    bool QueryResult::ok() const {
        return status_.ok();
    }

    const Status& QueryResult::status() const {
        return status_;
    }

    const std::string& QueryResult::message() const {
        return message_;
    }

    size_t QueryResult::row_count() const {
        return rows_.size();
    }

    const std::vector<std::string>& QueryResult::columns() const {
        return columns_;
    }

    const std::vector<Row>& QueryResult::rows() const {
        return rows_;
    }

    std::vector<Row>::const_iterator QueryResult::begin() const {
        return rows_.cbegin();
    }

    std::vector<Row>::const_iterator QueryResult::end() const {
        return rows_.cend();
    }

    // --- Factory Methods ---

    QueryResult QueryResult::Success(std::string msg) {
        QueryResult res;
        res.with_message(std::move(msg));
        return res;
    }

    QueryResult QueryResult::Error(Status status) {
        return QueryResult(std::move(status));
    }

    QueryResult QueryResult::FromData(std::vector<std::string> cols, std::vector<std::vector<std::string>> raw_rows) {
        std::vector<Row> rows;
        rows.reserve(raw_rows.size());
        
        for (auto& raw_row : raw_rows) {
            rows.emplace_back(Row(std::move(raw_row)));
        }
        
        // Используем конструктор (columns, rows)
        return QueryResult(std::move(cols), std::move(rows));
    }

} // namespace datyre
