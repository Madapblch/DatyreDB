#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <variant>
#include <optional>
#include <memory>

namespace datyredb {

// ============================================================================
// Basic Types
// ============================================================================

using PageId = uint32_t;
using FrameId = uint32_t;
using RecordId = uint64_t;
using TransactionId = uint64_t;
using TableId = uint32_t;

constexpr PageId INVALID_PAGE_ID = UINT32_MAX;
constexpr size_t PAGE_SIZE = 4096;

// ============================================================================
// Value Types
// ============================================================================

using Value = std::variant<
    std::monostate,  // NULL
    bool,
    int32_t,
    int64_t,
    double,
    std::string
>;

// ============================================================================
// Statistics Structures
// ============================================================================

struct Statistics {
    size_t table_count = 0;
    size_t total_records = 0;
    size_t total_size = 0;
    size_t index_count = 0;
    int uptime_seconds = 0;
};

struct TableStatistics {
    size_t record_count = 0;
    size_t size_bytes = 0;
};

struct DetailedStatistics : Statistics {
    std::string version = "1.0.0";
    size_t memory_used = 0;
    size_t memory_total = 0;
    size_t disk_used = 0;
    size_t disk_total = 0;
    float cpu_usage = 0.0f;
    int queries_per_second = 0;
    float avg_query_time = 0.0f;
    int active_connections = 0;
    float cache_hit_ratio = 0.0f;
};

// ============================================================================
// Query Result
// ============================================================================

class QueryResult {
public:
    QueryResult() = default;
    
    bool has_data() const { return !data_.empty(); }
    int affected_rows() const { return affected_rows_; }
    const std::vector<std::string>& columns() const { return columns_; }
    const std::vector<std::vector<std::string>>& rows() const { return data_; }
    
    void set_columns(const std::vector<std::string>& cols) { columns_ = cols; }
    void add_row(const std::vector<std::string>& row) { data_.push_back(row); }
    void set_affected_rows(int count) { affected_rows_ = count; }
    
private:
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> data_;
    int affected_rows_ = 0;
};

} // namespace datyredb
