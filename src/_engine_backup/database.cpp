#include "engine/database.hpp"
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iomanip>

namespace datyredb {

Database::Database() 
    : start_time_(std::chrono::steady_clock::now()) {
    
    // Create sample tables for demonstration
    tables_["users"] = {
        {"id", "name", "email"},
        {
            {"1", "Alice", "alice@example.com"},
            {"2", "Bob", "bob@example.com"},
            {"3", "Charlie", "charlie@example.com"}
        }
    };
    
    tables_["products"] = {
        {"id", "name", "price"},
        {
            {"1", "Laptop", "999.99"},
            {"2", "Phone", "499.99"},
            {"3", "Tablet", "299.99"}
        }
    };
}

Database::~Database() = default;

QueryResult Database::execute_query(const std::string& sql) {
    QueryResult result;
    
    // Simple SQL parsing (for demonstration)
    std::string query = sql;
    std::transform(query.begin(), query.end(), query.begin(), ::toupper);
    
    if (query.find("SELECT") == 0) {
        // Parse table name from "SELECT * FROM table"
        auto from_pos = query.find("FROM");
        if (from_pos != std::string::npos) {
            std::string rest = sql.substr(from_pos + 5);
            std::istringstream iss(rest);
            std::string table_name;
            iss >> table_name;
            
            // Remove any trailing characters
            while (!table_name.empty() && !std::isalnum(table_name.back())) {
                table_name.pop_back();
            }
            
            return select(table_name);
        }
    } else if (query.find("SHOW TABLES") == 0) {
        result.set_columns({"table_name"});
        for (const auto& table : list_tables()) {
            result.add_row({table});
        }
        return result;
    }
    
    return result;
}

std::vector<std::string> Database::list_tables() {
    std::shared_lock lock(mutex_);
    
    std::vector<std::string> result;
    result.reserve(tables_.size());
    
    for (const auto& [name, _] : tables_) {
        result.push_back(name);
    }
    
    std::sort(result.begin(), result.end());
    return result;
}

bool Database::create_table(const std::string& name, const std::vector<std::string>& columns) {
    std::unique_lock lock(mutex_);
    
    if (tables_.find(name) != tables_.end()) {
        return false;  // Table already exists
    }
    
    tables_[name] = {columns, {}};
    return true;
}

bool Database::drop_table(const std::string& name) {
    std::unique_lock lock(mutex_);
    return tables_.erase(name) > 0;
}

bool Database::insert(const std::string& table, const std::vector<std::string>& values) {
    std::unique_lock lock(mutex_);
    
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return false;
    }
    
    it->second.rows.push_back(values);
    return true;
}

QueryResult Database::select(const std::string& table, const std::string& where) {
    std::shared_lock lock(mutex_);
    
    QueryResult result;
    
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return result;
    }
    
    result.set_columns(it->second.columns);
    
    for (const auto& row : it->second.rows) {
        result.add_row(row);
    }
    
    return result;
}

bool Database::update(const std::string& table, const std::string& set, const std::string& where) {
    std::unique_lock lock(mutex_);
    
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return false;
    }
    
    // TODO: Implement actual UPDATE logic
    return true;
}

bool Database::remove(const std::string& table, const std::string& where) {
    std::unique_lock lock(mutex_);
    
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return false;
    }
    
    // TODO: Implement actual DELETE logic
    return true;
}

Statistics Database::get_statistics() {
    std::shared_lock lock(mutex_);
    
    Statistics stats;
    stats.table_count = tables_.size();
    
    for (const auto& [_, table] : tables_) {
        stats.total_records += table.rows.size();
    }
    
    stats.index_count = tables_.size();  // One index per table
    stats.total_size = stats.total_records * 100;  // Approximate
    
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    stats.uptime_seconds = static_cast<int>(uptime.count());
    
    return stats;
}

DetailedStatistics Database::get_detailed_statistics() {
    auto basic = get_statistics();
    
    DetailedStatistics stats;
    static_cast<Statistics&>(stats) = basic;
    
    stats.version = "1.0.0";
    stats.memory_used = 100ULL * 1024 * 1024;      // 100MB
    stats.memory_total = 1024ULL * 1024 * 1024;    // 1GB
    stats.disk_used = 500ULL * 1024 * 1024;        // 500MB
    stats.disk_total = 10ULL * 1024 * 1024 * 1024; // 10GB
    stats.cpu_usage = 15.5f;
    stats.queries_per_second = 1000;
    stats.avg_query_time = 0.5f;
    stats.active_connections = 5;
    stats.cache_hit_ratio = 95.5f;
    
    return stats;
}

TableStatistics Database::get_table_statistics(const std::string& table) {
    std::shared_lock lock(mutex_);
    
    TableStatistics stats;
    
    auto it = tables_.find(table);
    if (it != tables_.end()) {
        stats.record_count = it->second.rows.size();
        stats.size_bytes = stats.record_count * 100;  // Approximate
    }
    
    return stats;
}

std::string Database::create_backup() {
    std::shared_lock lock(mutex_);
    
    // Create backup directory if not exists
    std::filesystem::create_directories("backups");
    
    // Generate backup filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream filename;
    filename << "backups/datyredb_backup_"
             << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
             << ".sql";
    
    std::string backup_path = filename.str();
    
    // Write backup file
    std::ofstream file(backup_path);
    if (!file.is_open()) {
        return "";
    }
    
    file << "-- DatyreDB Backup\n";
    file << "-- Created: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n\n";
    
    for (const auto& [table_name, table_data] : tables_) {
        // CREATE TABLE statement
        file << "CREATE TABLE " << table_name << " (";
        for (size_t i = 0; i < table_data.columns.size(); ++i) {
            if (i > 0) file << ", ";
            file << table_data.columns[i] << " TEXT";
        }
        file << ");\n";
        
        // INSERT statements
        for (const auto& row : table_data.rows) {
            file << "INSERT INTO " << table_name << " VALUES (";
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) file << ", ";
                file << "'" << row[i] << "'";
            }
            file << ");\n";
        }
        
        file << "\n";
    }
    
    return backup_path;
}

} // namespace datyredb
