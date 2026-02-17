#pragma once

#include "storage/storage_types.hpp"
#include "storage/disk_manager.hpp"
#include "storage/buffer_pool.hpp"
#include "storage/wal.hpp"
#include "storage/checkpoint.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <memory>
#include <cstdint>
#include <filesystem>

namespace datyredb {

class StorageEngine {
public:
    /// Конфигурация
    struct Config {
        std::string data_path = "./data";
        std::size_t buffer_pool_pages = 10000;  // ~40 MB при 4KB страницах
        storage::CheckpointConfig checkpoint;
    };
    
    StorageEngine();
    explicit StorageEngine(Config config);
    ~StorageEngine();

    // Запретить копирование
    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;

    bool initialize();
    void shutdown();

    // ========================================================================
    // Table operations
    // ========================================================================
    
    bool create_table(const std::string& name, const std::vector<std::string>& columns);
    bool drop_table(const std::string& name);
    std::vector<std::string> list_tables() const;
    std::vector<std::string> get_table_columns(const std::string& table) const;

    // ========================================================================
    // Data operations
    // ========================================================================
    
    bool insert(const std::string& table, const std::vector<std::string>& values);
    std::vector<std::vector<std::string>> select(const std::string& table);
    bool update(const std::string& table, std::size_t row_id, 
                const std::vector<std::string>& values);
    bool remove(const std::string& table, std::size_t row_id);

    // ========================================================================
    // Checkpoint API
    // ========================================================================
    
    /// Ручной вызов checkpoint
    void checkpoint();
    
    /// Проверка давления памяти (вызывается перед транзакцией)
    bool check_memory_pressure();

    // ========================================================================
    // Statistics
    // ========================================================================
    
    std::size_t table_count() const;
    std::size_t total_records() const;
    std::size_t total_size() const;
    std::size_t index_count() const;
    std::size_t memory_usage() const;
    std::size_t disk_usage() const;
    float cache_hit_ratio() const;

    std::size_t table_record_count(const std::string& table) const;
    std::size_t table_size(const std::string& table) const;

    // ========================================================================
    // Metrics access
    // ========================================================================
    
    const storage::CheckpointMetrics* metrics() const { 
        return metrics_.get(); 
    }
    
    // Checkpoint stats
    std::size_t dirty_page_count() const;
    std::size_t buffer_pool_usage() const;
    uint64_t wal_size() const;
    uint64_t checkpoint_count() const;

    // ========================================================================
    // Backup
    // ========================================================================
    
    bool create_backup(const std::string& path);

private:
    // In-memory table structure (временно, пока нет B-tree)
    struct Table {
        std::vector<std::string> columns;
        std::vector<std::vector<std::string>> rows;
        std::size_t size_bytes = 0;
    };

    /// Вычислить размер таблицы в байтах
    static std::size_t calculate_table_size(const Table& table);

    Config config_;
    bool initialized_ = false;
    
    // Storage subsystems
    std::shared_ptr<storage::CheckpointMetrics> metrics_;
    std::shared_ptr<storage::DiskManager> disk_manager_;
    std::shared_ptr<storage::BufferPool> buffer_pool_;
    std::shared_ptr<storage::WriteAheadLog> wal_;
    std::shared_ptr<storage::CheckpointManager> checkpoint_manager_;

    // In-memory tables
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Table> tables_;

    // Statistics
    mutable uint64_t cache_hits_ = 0;
    mutable uint64_t cache_misses_ = 0;
};

} // namespace datyredb
