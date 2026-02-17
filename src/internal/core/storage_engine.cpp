// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Storage Engine Implementation                                    ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "storage_engine.hpp"

#include <fmt/core.h>
#include <filesystem>

namespace datyredb::core {

StorageEngine::StorageEngine()
    : StorageEngine(Config{})
{
}

StorageEngine::StorageEngine(Config config)
    : config_(std::move(config))
{
}

StorageEngine::~StorageEngine() {
    shutdown();
}

// ==============================================================================
// Lifecycle
// ==============================================================================

Status StorageEngine::initialize() {
    if (initialized_) {
        return Ok();
    }
    
    // Create metrics
    metrics_ = std::make_shared<storage::CheckpointMetrics>();
    
    // Initialize disk manager
    disk_manager_ = std::make_shared<storage::DiskManager>(
        std::filesystem::path(config_.data_directory)
    );
    
    if (auto status = disk_manager_->open(); !status) {
        return Err(status.error().code(),
            fmt::format("Failed to initialize disk manager: {}", 
                status.error().message()));
    }
    
    // Initialize WAL
    auto wal_path = std::filesystem::path(config_.data_directory) / "wal";
    wal_ = std::make_shared<storage::WriteAheadLog>(
        wal_path,
        config_.wal_segment_size,
        metrics_
    );
    
    if (auto status = wal_->open(); !status) {
        return Err(status.error().code(),
            fmt::format("Failed to initialize WAL: {}",
                status.error().message()));
    }
    
    // Initialize buffer pool
    buffer_pool_ = std::make_shared<storage::BufferPool>(
        config_.buffer_pool_size,
        disk_manager_,
        metrics_
    );
    
    // Initialize checkpoint manager
    checkpoint_manager_ = std::make_shared<storage::CheckpointManager>(
        config_.checkpoint,
        buffer_pool_,
        wal_,
        metrics_
    );
    
    checkpoint_manager_->start();
    
    initialized_ = true;
    return Ok();
}

void StorageEngine::shutdown() {
    if (!initialized_) {
        return;
    }
    
    // Stop checkpoint manager (will do final checkpoint)
    if (checkpoint_manager_) {
        checkpoint_manager_->stop();
        checkpoint_manager_.reset();
    }
    
    // Flush buffer pool
    buffer_pool_.reset();
    
    // Close WAL
    if (wal_) {
        wal_->close();
        wal_.reset();
    }
    
    // Close disk manager
    if (disk_manager_) {
        disk_manager_->close();
        disk_manager_.reset();
    }
    
    initialized_ = false;
}

// ==============================================================================
// Page Operations
// ==============================================================================

Result<Page*> StorageEngine::get_page(PageId page_id) {
    if (!initialized_) {
        return Err<Page*>(ErrorCode::InternalError, "Storage engine not initialized");
    }
    
    // Check memory pressure. Explicit cast to void to suppress nodiscard warning.
    if (checkpoint_manager_) {
        (void)checkpoint_manager_->check_pressure();
    }
    
    return buffer_pool_->fetch_page(page_id);
}

Result<Page*> StorageEngine::create_page() {
    if (!initialized_) {
        return Err<Page*>(ErrorCode::InternalError, "Storage engine not initialized");
    }
    
    // Check memory pressure. Explicit cast to void to suppress nodiscard warning.
    if (checkpoint_manager_) {
        (void)checkpoint_manager_->check_pressure();
    }
    
    return buffer_pool_->new_page();
}

Status StorageEngine::release_page(PageId page_id, bool modified) {
    if (!initialized_) {
        return Err(ErrorCode::InternalError, "Storage engine not initialized");
    }
    
    return buffer_pool_->unpin_page(page_id, modified);
}

// ==============================================================================
// Checkpoint
// ==============================================================================

void StorageEngine::checkpoint() {
    if (checkpoint_manager_) {
        checkpoint_manager_->request_checkpoint();
    }
}

Status StorageEngine::checkpoint_sync() {
    if (!checkpoint_manager_) {
        return Err(ErrorCode::InternalError, "Checkpoint manager not initialized");
    }
    
    return checkpoint_manager_->checkpoint_sync();
}

// ==============================================================================
// Statistics
// ==============================================================================

std::size_t StorageEngine::buffer_pool_size() const noexcept {
    return buffer_pool_ ? buffer_pool_->size() : 0;
}

std::size_t StorageEngine::dirty_page_count() const noexcept {
    return buffer_pool_ ? buffer_pool_->dirty_count() : 0;
}

std::uint64_t StorageEngine::wal_size() const noexcept {
    return wal_ ? wal_->size() : 0;
}

Lsn StorageEngine::current_lsn() const noexcept {
    return wal_ ? wal_->current_lsn() : INVALID_LSN;
}

PageId StorageEngine::page_count() const noexcept {
    return disk_manager_ ? disk_manager_->page_count() : 0;
}

} // namespace datyredb::core
