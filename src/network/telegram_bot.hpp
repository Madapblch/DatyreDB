#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <fstream>
#include <iomanip>
#include <ctime>

namespace datyredb {

// Forward declarations для Database
namespace database {
    
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
        size_t memory_used = 1024 * 1024 * 100;
        size_t memory_total = 1024 * 1024 * 1024;
        size_t disk_used = 1024 * 1024 * 500;
        size_t disk_total = 1024 * 1024 * 1024 * 10;
        float cpu_usage = 15.5;
        int queries_per_second = 1000;
        float avg_query_time = 0.5;
        int active_connections = 5;
        float cache_hit_ratio = 95.5;
    };

    class QueryResult {
    public:
        bool has_data() const { return !data_.empty(); }
        int affected_rows() const { return affected_rows_; }
        const std::vector<std::string>& columns() const { return columns_; }
        const std::vector<std::vector<std::string>>& rows() const { return data_; }
        
    private:
        std::vector<std::string> columns_ = {"id", "name", "value"};
        std::vector<std::vector<std::string>> data_ = {
            {"1", "test", "value1"},
            {"2", "demo", "value2"}
        };
        int affected_rows_ = 0;
    };

    class Database {
    public:
        Statistics get_statistics() { 
            Statistics stats;
            stats.table_count = 5;
            stats.total_records = 1000;
            stats.total_size = 1024 * 1024;
            stats.index_count = 3;
            stats.uptime_seconds = 3600;
            return stats;
        }
        
        DetailedStatistics get_detailed_statistics() { 
            return DetailedStatistics{}; 
        }
        
        TableStatistics get_table_statistics(const std::string& table) { 
            TableStatistics stats;
            stats.record_count = 100;
            stats.size_bytes = 1024 * 10;
            return stats;
        }
        
        std::vector<std::string> list_tables() { 
            return {"users", "products", "orders", "logs", "settings"}; 
        }
        
        QueryResult execute_query(const std::string& query) { 
            return QueryResult{}; 
        }
        
        std::string create_backup() { 
            return "/tmp/backup_" + std::to_string(std::time(nullptr)) + ".db"; 
        }
    };
}

namespace network {

struct TelegramMessage {
    int64_t chat_id = 0;
    int64_t from_id = 0;
    int64_t message_id = 0;
    std::string username;
    std::string text;
};

class TelegramBot {
public:
    TelegramBot(const std::string& token, database::Database* db = nullptr);
    ~TelegramBot();
    
    void start();
    void stop();
    
    void send_message(int64_t chat_id, const std::string& text, bool parse_markdown = false);
    
private:
    using CommandHandler = std::function<void(const TelegramMessage&)>;
    
    // Core functionality
    void init_commands();
    void bot_loop();
    void process_updates();
    void handle_message(const TelegramMessage& msg);
    
    // API methods
    bool test_connection();
    std::string make_request(const std::string& method, const std::string& params = "");
    TelegramMessage parse_message(const std::string& json);
    
    // Helper methods
    std::string format_bytes(size_t bytes);
    std::string format_duration(int seconds);
    std::string format_query_result(const database::QueryResult& result);
    std::string get_current_timestamp();
    size_t get_file_size(const std::string& path);
    
    // Member variables
    std::string bot_token_;
    database::Database* db_;
    
    std::unordered_map<std::string, CommandHandler> commands_;
    
    std::atomic<bool> running_;
    std::thread bot_thread_;
    int update_offset_;
};

} // namespace network
} // namespace datyredb#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <fstream>
#include <iomanip>
#include <ctime>

namespace datyredb {

// Forward declarations для Database
namespace database {
    
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
        size_t memory_used = 1024 * 1024 * 100;
        size_t memory_total = 1024 * 1024 * 1024;
        size_t disk_used = 1024 * 1024 * 500;
        size_t disk_total = 1024 * 1024 * 1024 * 10;
        float cpu_usage = 15.5;
        int queries_per_second = 1000;
        float avg_query_time = 0.5;
        int active_connections = 5;
        float cache_hit_ratio = 95.5;
    };

    class QueryResult {
    public:
        bool has_data() const { return !data_.empty(); }
        int affected_rows() const { return affected_rows_; }
        const std::vector<std::string>& columns() const { return columns_; }
        const std::vector<std::vector<std::string>>& rows() const { return data_; }
        
    private:
        std::vector<std::string> columns_ = {"id", "name", "value"};
        std::vector<std::vector<std::string>> data_ = {
            {"1", "test", "value1"},
            {"2", "demo", "value2"}
        };
        int affected_rows_ = 0;
    };

    class Database {
    public:
        Statistics get_statistics() { 
            Statistics stats;
            stats.table_count = 5;
            stats.total_records = 1000;
            stats.total_size = 1024 * 1024;
            stats.index_count = 3;
            stats.uptime_seconds = 3600;
            return stats;
        }
        
        DetailedStatistics get_detailed_statistics() { 
            return DetailedStatistics{}; 
        }
        
        TableStatistics get_table_statistics(const std::string& table) { 
            TableStatistics stats;
            stats.record_count = 100;
            stats.size_bytes = 1024 * 10;
            return stats;
        }
        
        std::vector<std::string> list_tables() { 
            return {"users", "products", "orders", "logs", "settings"}; 
        }
        
        QueryResult execute_query(const std::string& query) { 
            return QueryResult{}; 
        }
        
        std::string create_backup() { 
            return "/tmp/backup_" + std::to_string(std::time(nullptr)) + ".db"; 
        }
    };
}

namespace network {

struct TelegramMessage {
    int64_t chat_id = 0;
    int64_t from_id = 0;
    int64_t message_id = 0;
    std::string username;
    std::string text;
};

class TelegramBot {
public:
    TelegramBot(const std::string& token, database::Database* db = nullptr);
    ~TelegramBot();
    
    void start();
    void stop();
    
    void send_message(int64_t chat_id, const std::string& text, bool parse_markdown = false);
    
private:
    using CommandHandler = std::function<void(const TelegramMessage&)>;
    
    // Core functionality
    void init_commands();
    void bot_loop();
    void process_updates();
    void handle_message(const TelegramMessage& msg);
    
    // API methods
    bool test_connection();
    std::string make_request(const std::string& method, const std::string& params = "");
    TelegramMessage parse_message(const std::string& json);
    
    // Helper methods
    std::string format_bytes(size_t bytes);
    std::string format_duration(int seconds);
    std::string format_query_result(const database::QueryResult& result);
    std::string get_current_timestamp();
    size_t get_file_size(const std::string& path);
    
    // Member variables
    std::string bot_token_;
    database::Database* db_;
    
    std::unordered_map<std::string, CommandHandler> commands_;
    
    std::atomic<bool> running_;
    std::thread bot_thread_;
    int update_offset_;
};

} // namespace network
} // namespace datyredb
