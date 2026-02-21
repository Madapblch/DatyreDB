#pragma once

#include "common/types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <ctime>

namespace datyredb {

class Database {
public:
    Database();
    ~Database();
    
    // Query execution
    QueryResult execute_query(const std::string& sql);
    
    // Table operations
    std::vector<std::string> list_tables();
    bool create_table(const std::string& name, const std::vector<std::string>& columns);
    bool drop_table(const std::string& name);
    
    // CRUD operations
    bool insert(const std::string& table, const std::vector<std::string>& values);
    QueryResult select(const std::string& table, const std::string& where = "");
    bool update(const std::string& table, const std::string& set, const std::string& where);
    bool remove(const std::string& table, const std::string& where);
    
    // Statistics
    Statistics get_statistics();
    DetailedStatistics get_detailed_statistics();
    TableStatistics get_table_statistics(const std::string& table);
    
    // Backup
    std::string create_backup();
    
private:
    struct TableData {
        std::vector<std::string> columns;
        std::vector<std::vector<std::string>> rows;
    };
    
    std::unordered_map<std::string, TableData> tables_;
    mutable std::shared_mutex mutex_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace datyredb
