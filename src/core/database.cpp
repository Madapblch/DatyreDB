#include "core/database.hpp"
#include "core/storage_engine.hpp"
#include "core/query_executor.hpp"
#include "utils/logger.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace datyredb {

Database::Database() 
    : start_time_(std::chrono::steady_clock::now())
    , impl_(std::make_unique<Impl>())
    , storage_(nullptr)
    , executor_(nullptr) {
    Logger::debug("Database object created");
}

Database::~Database() {
    if (storage_ || executor_) {
        shutdown();
    }
}

bool Database::initialize() {
    try {
        Logger::info("Initializing database components...");
        
        // Initialize storage engine
        storage_ = std::make_unique<StorageEngine>();
        if (!storage_->initialize()) {
            Logger::error("Failed to initialize storage engine");
            return false;
        }
        
        // Initialize query executor
        executor_ = std::make_unique<QueryExecutor>(storage_.get());
        if (!executor_->initialize()) {
            Logger::error("Failed to initialize query executor");
            return false;
        }
        
        Logger::info("Database initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Exception during initialization: {}", e.what());
        return false;
    }
}

void Database::shutdown() {
    Logger::info("Shutting down database...");
    
    if (executor_) {
        executor_->shutdown();
        executor_.reset();
    }
    
    if (storage_) {
        storage_->shutdown();
        storage_.reset();
    }
    
    Logger::info("Database shutdown complete");
}

QueryResult Database::execute_query(const std::string& sql) {
    auto start = std::chrono::high_resolution_clock::now();
    
    QueryResult result;
    
    try {
        if (!executor_) {
            result.success = false;
            result.error_message = "Database not initialized";
            return result;
        }
        
        result = executor_->execute(sql);
        impl_->query_counter++;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Безопасное преобразование типов
    auto time_count = result.execution_time.count();
    if (time_count >= 0) {
        impl_->total_query_time += static_cast<uint64_t>(time_count);
    }
    
    return result;
}

std::vector<std::string> Database::list_tables() {
    if (!storage_) return {};
    return storage_->list_tables();
}

Statistics Database::get_statistics() {
    Statistics stats;
    
    if (storage_) {
        stats.table_count = storage_->table_count();
        stats.total_records = storage_->total_records();
        stats.total_size = storage_->total_size();
        stats.index_count = storage_->index_count();
    }
    
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    stats.uptime_seconds = static_cast<int>(uptime.count());
    
    return stats;
}

DetailedStatistics Database::get_detailed_statistics() {
    DetailedStatistics stats;
    static_cast<Statistics&>(stats) = get_statistics();
    
    // Add detailed metrics
    stats.memory_used = storage_ ? storage_->memory_usage() : 0;
    stats.memory_total = 1024ULL * 1024 * 1024;
    stats.disk_used = storage_ ? storage_->disk_usage() : 0;
    stats.disk_total = 10ULL * 1024 * 1024 * 1024;
    
    // Calculate performance metrics with safe type conversions
    uint64_t query_count = impl_->query_counter.load();
    if (query_count > 0) {
        int uptime = std::max(1, stats.uptime_seconds);
        stats.queries_per_second = static_cast<int>(query_count / static_cast<uint64_t>(uptime));
        
        uint64_t total_time = impl_->total_query_time.load();
        stats.avg_query_time = static_cast<float>(total_time) / static_cast<float>(query_count);
    }
    
    stats.cache_hit_ratio = storage_ ? storage_->cache_hit_ratio() : 0.0f;
    
    return stats;
}

TableStatistics Database::get_table_statistics(const std::string& table) {
    TableStatistics stats;
    
    if (storage_) {
        stats.record_count = storage_->table_record_count(table);
        stats.size_bytes = storage_->table_size(table);
    }
    
    return stats;
}

std::string Database::create_backup() {
    std::filesystem::create_directories("backups");
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream filename;
    filename << "backups/datyredb_backup_"
             << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
             << ".ddb";
    
    std::string backup_path = filename.str();
    
    if (storage_) {
        if (!storage_->create_backup(backup_path)) {
            Logger::error("Failed to create backup");
            return "";
        }
    }
    
    Logger::info("Backup created: {}", backup_path);
    return backup_path;
}

} // namespace datyredb
