#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>

namespace datyredb {

// Forward declarations
class StorageEngine;
class QueryExecutor;

// Result type for queries
struct QueryResult {
    bool success = false;
    std::string error_message;
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> columns;
    size_t affected_rows = 0;
    std::chrono::milliseconds execution_time{0};
    
    bool has_data() const { return !rows.empty(); }
    size_t row_count() const { return rows.size(); }
};

// Statistics structures
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

class Database {
public:
    Database();
    ~Database();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    
    // Query execution
    QueryResult execute_query(const std::string& sql);
    
    // Table operations
    std::vector<std::string> list_tables();
    
    // Statistics
    Statistics get_statistics();
    DetailedStatistics get_detailed_statistics();
    TableStatistics get_table_statistics(const std::string& table);
    
    // Backup
    std::string create_backup();
    
private:
    struct Impl {
        std::atomic<uint64_t> query_counter{0};
        std::atomic<uint64_t> total_query_time{0};
    };
    
    // ВАЖНО: Порядок объявления должен совпадать с порядком инициализации
    std::chrono::steady_clock::time_point start_time_;
    std::unique_ptr<Impl> impl_;
    std::unique_ptr<StorageEngine> storage_;
    std::unique_ptr<QueryExecutor> executor_;
};

} // namespace datyredb
