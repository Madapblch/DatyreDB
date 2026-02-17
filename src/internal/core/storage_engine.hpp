// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Storage Engine                                                   ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#pragma once

#include "internal/storage/types.hpp"
#include "internal/storage/disk_manager.hpp"
#include "internal/storage/buffer_pool.hpp"
#include "internal/storage/wal.hpp"
#include "internal/storage/checkpoint.hpp"

#include <memory>
#include <string>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace datyredb::core {

using namespace storage;

/// Main storage engine coordinating all storage subsystems
class StorageEngine {
public:
    // ==========================================================================
    // Configuration
    // ==========================================================================
    
    struct Config {
        std::string data_directory = "./data";
        std::size_t buffer_pool_size = 10000;  // Pages
        std::size_t wal_segment_size = 64 * 1024 * 1024;  // 64 MB
        storage::CheckpointConfig checkpoint;
    };
    
    // ==========================================================================
    // Construction
    // ==========================================================================
    
    StorageEngine();
    explicit StorageEngine(Config config);
    ~StorageEngine();
    
    // Non-copyable, non-movable
    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;
    StorageEngine(StorageEngine&&) = delete;
    StorageEngine& operator=(StorageEngine&&) = delete;
    
    // ==========================================================================
    // Lifecycle
    // ==========================================================================
    
    /// Initialize storage engine
    [[nodiscard]] Status initialize();
    
    /// Shutdown storage engine
    void shutdown();
    
    /// Check if initialized
    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }
    
    // ==========================================================================
    // Page Operations (low-level)
    // ==========================================================================
    
    /// Get page for reading/writing
    [[nodiscard]] Result<Page*> get_page(PageId page_id);
    
    /// Create new page
    [[nodiscard]] Result<Page*> create_page();
    
    /// Release page after use
    [[nodiscard]] Status release_page(PageId page_id, bool modified);
    
    // ==========================================================================
    // Checkpoint
    // ==========================================================================
    
    /// Request checkpoint
    void checkpoint();
    
    /// Synchronous checkpoint
    [[nodiscard]] Status checkpoint_sync();
    
    // ==========================================================================
    // Statistics
    // ==========================================================================
    
    /// Get metrics
    [[nodiscard]] const storage::CheckpointMetrics* metrics() const noexcept {
        return metrics_.get();
    }
    
    /// Buffer pool stats
    [[nodiscard]] std::size_t buffer_pool_size() const noexcept;
    [[nodiscard]] std::size_t dirty_page_count() const noexcept;
    
    /// WAL stats
    [[nodiscard]] std::uint64_t wal_size() const noexcept;
    [[nodiscard]] Lsn current_lsn() const noexcept;
    
    /// Total pages
    [[nodiscard]] PageId page_count() const noexcept;
    
private:
    Config config_;
    bool initialized_{false};
    
    std::shared_ptr<storage::CheckpointMetrics> metrics_;
    std::shared_ptr<storage::DiskManager> disk_manager_;
    std::shared_ptr<storage::BufferPool> buffer_pool_;
    std::shared_ptr<storage::WriteAheadLog> wal_;
    std::shared_ptr<storage::CheckpointManager> checkpoint_manager_;
};

} // namespace datyredb::core
