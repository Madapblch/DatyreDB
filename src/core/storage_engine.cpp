#include "core/storage_engine.hpp"
#include "utils/logger.hpp"

#include <numeric>
#include <algorithm>
#include <cstring>
#include <fstream>

namespace datyredb {

StorageEngine::StorageEngine() 
    : StorageEngine(Config{})
{
}

StorageEngine::StorageEngine(Config config)
    : config_(std::move(config))
{
    Logger::debug("StorageEngine created with data_path={}", config_.data_path);
}

StorageEngine::~StorageEngine() {
    shutdown();
}

bool StorageEngine::initialize() {
    if (initialized_) {
        Logger::warn("StorageEngine already initialized");
        return true;
    }
    
    Logger::info("Initializing storage engine...");
    Logger::info("  Data path: {}", config_.data_path);
    Logger::info("  Buffer pool: {} pages ({} MB)", 
                 config_.buffer_pool_pages,
                 (config_.buffer_pool_pages * storage::PAGE_SIZE) / (1024 * 1024));
    
    // =========================================================================
    // 1. Создаём shared метрики
    // =========================================================================
    metrics_ = std::make_shared<storage::CheckpointMetrics>();
    
    // =========================================================================
    // 2. Инициализируем Disk Manager
    // =========================================================================
    disk_manager_ = std::make_shared<storage::DiskManager>(
        std::filesystem::path(config_.data_path)
    );
    
    if (!disk_manager_->initialize()) {
        Logger::error("Failed to initialize DiskManager");
        return false;
    }
    
    // =========================================================================
    // 3. Инициализируем WAL
    // =========================================================================
    auto wal_path = std::filesystem::path(config_.data_path) / "wal";
    wal_ = std::make_shared<storage::WriteAheadLog>(
        wal_path,
        64 * 1024 * 1024,  // 64 MB segments
        metrics_
    );
    
    if (!wal_->initialize()) {
        Logger::error("Failed to initialize WAL");
        return false;
    }
    
    // =========================================================================
    // 4. Инициализируем Buffer Pool
    // =========================================================================
    buffer_pool_ = std::make_shared<storage::BufferPool>(
        config_.buffer_pool_pages,
        disk_manager_,
        metrics_
    );
    
    // =========================================================================
    // 5. Инициализируем Checkpoint Manager
    // =========================================================================
    checkpoint_manager_ = std::make_shared<storage::CheckpointManager>(
        config_.checkpoint,
        buffer_pool_,
        wal_,
        metrics_
    );
    
    // Запускаем фоновый поток checkpoint'ов
    checkpoint_manager_->start();
    
    // =========================================================================
    // 6. Создаём demo таблицы (для тестирования)
    // =========================================================================
    create_table("users", {"id", "name", "email", "created_at"});
    insert("users", {"1", "Alice", "alice@example.com", "2024-01-01"});
    insert("users", {"2", "Bob", "bob@example.com", "2024-01-02"});
    insert("users", {"3", "Charlie", "charlie@example.com", "2024-01-03"});

    create_table("products", {"id", "name", "price", "stock"});
    insert("products", {"1", "Laptop", "999.99", "10"});
    insert("products", {"2", "Mouse", "29.99", "50"});
    insert("products", {"3", "Keyboard", "79.99", "30"});

    create_table("orders", {"id", "user_id", "product_id", "quantity", "total"});
    insert("orders", {"1", "1", "1", "1", "999.99"});
    insert("orders", {"2", "2", "2", "2", "59.98"});
    
    initialized_ = true;
    
    Logger::info("Storage engine initialized successfully");
    Logger::info("  Tables: {}", tables_.size());
    Logger::info("  Buffer pool capacity: {} pages", buffer_pool_->capacity());
    Logger::info("  WAL size: {} bytes", wal_->current_size());
    
    return true;
}

void StorageEngine::shutdown() {
    if (!initialized_) {
        return;
    }
    
    Logger::info("Shutting down storage engine...");
    
    // 1. Останавливаем checkpoint manager (он сделает финальный checkpoint)
    if (checkpoint_manager_) {
        checkpoint_manager_->shutdown();
        checkpoint_manager_.reset();
    }
    
    // 2. Закрываем buffer pool (flush все dirty pages)
    if (buffer_pool_) {
        buffer_pool_.reset();
    }
    
    // 3. Закрываем WAL
    if (wal_) {
        wal_->shutdown();
        wal_.reset();
    }
    
    // 4. Закрываем disk manager
    if (disk_manager_) {
        disk_manager_->shutdown();
        disk_manager_.reset();
    }
    
    // 5. Очищаем in-memory таблицы
    {
        std::unique_lock lock(mutex_);
        tables_.clear();
    }
    
    initialized_ = false;
    
    Logger::info("Storage engine shutdown complete");
}

// ============================================================================
// Table operations
// ============================================================================

bool StorageEngine::create_table(const std::string& name, 
                                  const std::vector<std::string>& columns) {
    std::unique_lock lock(mutex_);

    if (tables_.find(name) != tables_.end()) {
        Logger::warn("Table '{}' already exists", name);
        return false;
    }

    tables_[name] = Table{columns, {}, 0};
    
    Logger::info("Table '{}' created with {} columns", name, columns.size());
    return true;
}

bool StorageEngine::drop_table(const std::string& name) {
    std::unique_lock lock(mutex_);

    auto it = tables_.find(name);
    if (it == tables_.end()) {
        Logger::warn("Table '{}' not found", name);
        return false;
    }

    tables_.erase(it);
    
    Logger::info("Table '{}' dropped", name);
    return true;
}

std::vector<std::string> StorageEngine::list_tables() const {
    std::shared_lock lock(mutex_);

    std::vector<std::string> result;
    result.reserve(tables_.size());

    for (const auto& [name, table] : tables_) {
        (void)table;
        result.push_back(name);
    }

    return result;
}

std::vector<std::string> StorageEngine::get_table_columns(const std::string& table) const {
    std::shared_lock lock(mutex_);

    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return {};
    }

    return it->second.columns;
}

// ============================================================================
// Data operations
// ============================================================================

bool StorageEngine::insert(const std::string& table, 
                           const std::vector<std::string>& values) {
    // Проверяем давление памяти перед операцией
    if (checkpoint_manager_) {
        checkpoint_manager_->check_pressure();
    }
    
    std::unique_lock lock(mutex_);

    auto it = tables_.find(table);
    if (it == tables_.end()) {
        Logger::warn("Table '{}' not found for insert", table);
        return false;
    }

    auto& tbl = it->second;

    if (values.size() != tbl.columns.size()) {
        Logger::warn("Column count mismatch for table '{}': expected {}, got {}",
                     table, tbl.columns.size(), values.size());
        return false;
    }

    tbl.rows.push_back(values);
    tbl.size_bytes = calculate_table_size(tbl);

    // TODO: Записать в WAL для durability
    // LogRecord rec;
    // rec.type = storage::LogRecordType::INSERT;
    // wal_->append(rec);

    return true;
}

std::vector<std::vector<std::string>> StorageEngine::select(const std::string& table) {
    std::shared_lock lock(mutex_);

    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return {};
    }

    ++cache_hits_;  // Simplified cache tracking
    return it->second.rows;
}

bool StorageEngine::update(const std::string& table, 
                           std::size_t row_id,
                           const std::vector<std::string>& values) {
    if (checkpoint_manager_) {
        checkpoint_manager_->check_pressure();
    }
    
    std::unique_lock lock(mutex_);

    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return false;
    }

    auto& tbl = it->second;
    
    if (row_id >= tbl.rows.size()) {
        return false;
    }
    
    if (values.size() != tbl.columns.size()) {
        return false;
    }

    tbl.rows[row_id] = values;
    tbl.size_bytes = calculate_table_size(tbl);

    return true;
}

bool StorageEngine::remove(const std::string& table, std::size_t row_id) {
    if (checkpoint_manager_) {
        checkpoint_manager_->check_pressure();
    }
    
    std::unique_lock lock(mutex_);

    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return false;
    }

    auto& tbl = it->second;
    
    if (row_id >= tbl.rows.size()) {
        return false;
    }

    tbl.rows.erase(tbl.rows.begin() + static_cast<std::ptrdiff_t>(row_id));
    tbl.size_bytes = calculate_table_size(tbl);

    return true;
}

// ============================================================================
// Checkpoint API
// ============================================================================

void StorageEngine::checkpoint() {
    if (checkpoint_manager_) {
        Logger::info("Manual checkpoint requested");
        checkpoint_manager_->manual_checkpoint();
    }
}

bool StorageEngine::check_memory_pressure() {
    if (checkpoint_manager_) {
        return checkpoint_manager_->check_pressure();
    }
    return false;
}

// ============================================================================
// Statistics
// ============================================================================

std::size_t StorageEngine::table_count() const {
    std::shared_lock lock(mutex_);
    return tables_.size();
}

std::size_t StorageEngine::total_records() const {
    std::shared_lock lock(mutex_);
    
    std::size_t total = 0;
    for (const auto& [name, table] : tables_) {
        (void)name;
        total += table.rows.size();
    }
    return total;
}

std::size_t StorageEngine::total_size() const {
    std::shared_lock lock(mutex_);
    
    std::size_t total = 0;
    for (const auto& [name, table] : tables_) {
        (void)name;
        total += table.size_bytes;
    }
    return total;
}

std::size_t StorageEngine::index_count() const {
    // TODO: implement when indexes are added
    return 0;
}

std::size_t StorageEngine::memory_usage() const {
    std::size_t usage = total_size();
    
    if (buffer_pool_) {
        usage += buffer_pool_->page_count() * storage::PAGE_SIZE;
    }
    
    return usage;
}

std::size_t StorageEngine::disk_usage() const {
    if (disk_manager_) {
        return static_cast<std::size_t>(disk_manager_->data_file_size());
    }
    return 0;
}

float StorageEngine::cache_hit_ratio() const {
    uint64_t total = cache_hits_ + cache_misses_;
    if (total == 0) return 1.0f;
    return static_cast<float>(cache_hits_) / static_cast<float>(total);
}

std::size_t StorageEngine::table_record_count(const std::string& table) const {
    std::shared_lock lock(mutex_);
    
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return 0;
    }
    return it->second.rows.size();
}

std::size_t StorageEngine::table_size(const std::string& table) const {
    std::shared_lock lock(mutex_);
    
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return 0;
    }
    return it->second.size_bytes;
}

std::size_t StorageEngine::dirty_page_count() const {
    if (buffer_pool_) {
        return buffer_pool_->dirty_page_count();
    }
    return 0;
}

std::size_t StorageEngine::buffer_pool_usage() const {
    if (buffer_pool_) {
        return buffer_pool_->page_count();
    }
    return 0;
}

uint64_t StorageEngine::wal_size() const {
    if (wal_) {
        return wal_->current_size();
    }
    return 0;
}

uint64_t StorageEngine::checkpoint_count() const {
    if (metrics_) {
        return metrics_->checkpoint_count.load(std::memory_order_relaxed);
    }
    return 0;
}

// ============================================================================
// Backup
// ============================================================================

bool StorageEngine::create_backup(const std::string& path) {
    // Сначала делаем checkpoint для консистентности
    checkpoint();
    
    std::shared_lock lock(mutex_);
    
    try {
        std::filesystem::create_directories(path);
        
        // Копируем файл данных
        if (disk_manager_) {
            auto src = disk_manager_->data_path() / "data.db";
            auto dst = std::filesystem::path(path) / "data.db";
            
            if (std::filesystem::exists(src)) {
                std::filesystem::copy_file(src, dst, 
                    std::filesystem::copy_options::overwrite_existing);
            }
        }
        
        Logger::info("Backup created at {}", path);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Backup failed: {}", e.what());
        return false;
    }
}

// ============================================================================
// Private helpers
// ============================================================================

std::size_t StorageEngine::calculate_table_size(const Table& table) {
    std::size_t size = 0;
    
    // Columns
    for (const auto& col : table.columns) {
        size += col.size();
    }
    
    // Rows
    for (const auto& row : table.rows) {
        for (const auto& cell : row) {
            size += cell.size();
        }
    }
    
    return size;
}

} // namespace datyredb
