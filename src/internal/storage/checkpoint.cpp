// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Checkpoint Manager Implementation                                ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "checkpoint.hpp"

#include <fmt/core.h>
#include <algorithm>

namespace datyredb::storage {

CheckpointManager::CheckpointManager(
    CheckpointConfig config,
    std::shared_ptr<BufferPool> buffer_pool,
    std::shared_ptr<WriteAheadLog> wal,
    std::shared_ptr<CheckpointMetrics> metrics
)
    : config_(std::move(config))
    , buffer_pool_(std::move(buffer_pool))
    , wal_(std::move(wal))
    , metrics_(std::move(metrics))
    , last_checkpoint_time_(std::chrono::steady_clock::now())
{
}

CheckpointManager::~CheckpointManager() {
    stop();
}

// ==============================================================================
// Lifecycle
// ==============================================================================

void CheckpointManager::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return;  // Already running
    }
    
    background_thread_ = std::thread(&CheckpointManager::background_loop, this);
}

void CheckpointManager::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;  // Not running
    }
    
    // Wake up background thread
    {
        std::lock_guard lock(wake_mutex_);
        wake_cv_.notify_one();
    }
    
    // Unblock any waiting transactions
    {
        std::lock_guard lock(block_mutex_);
        blocking_ = false;
        block_cv_.notify_all();
    }
    
    if (background_thread_.joinable()) {
        background_thread_.join();
    }
    
    // Final checkpoint on shutdown
    [[maybe_unused]] auto status = do_checkpoint(CheckpointTrigger::Shutdown);
}

// ==============================================================================
// Manual Operations
// ==============================================================================

void CheckpointManager::request_checkpoint() {
    checkpoint_requested_.store(true, std::memory_order_release);
    
    std::lock_guard lock(wake_mutex_);
    wake_cv_.notify_one();
}

Status CheckpointManager::checkpoint_sync() {
    std::lock_guard lock(checkpoint_mutex_);
    return do_checkpoint(CheckpointTrigger::Manual);
}

// ==============================================================================
// Pressure Handling
// ==============================================================================

bool CheckpointManager::check_pressure() {
    if (!blocking_.load(std::memory_order_acquire)) {
        return false;
    }
    
    // Wait for checkpoint to complete
    std::unique_lock lock(block_mutex_);
    block_cv_.wait(lock, [this] {
        return !blocking_.load(std::memory_order_relaxed) || 
               !running_.load(std::memory_order_relaxed);
    });
    
    return true;
}

// ==============================================================================
// Background Thread
// ==============================================================================

void CheckpointManager::background_loop() {
    while (running_.load(std::memory_order_relaxed)) {
        // Wait for trigger or timeout
        {
            std::unique_lock lock(wake_mutex_);
            wake_cv_.wait_for(lock, std::chrono::seconds(1), [this] {
                return !running_.load(std::memory_order_relaxed) ||
                       checkpoint_requested_.load(std::memory_order_relaxed);
            });
        }
        
        if (!running_.load(std::memory_order_relaxed)) {
            break;
        }
        
        // Check for manual request
        if (checkpoint_requested_.exchange(false, std::memory_order_acq_rel)) {
            std::lock_guard lock(checkpoint_mutex_);
            [[maybe_unused]] auto status = do_checkpoint(CheckpointTrigger::Manual);
            continue;
        }
        
        // Check automatic triggers
        auto trigger = should_checkpoint();
        if (trigger) {
            std::lock_guard lock(checkpoint_mutex_);
            [[maybe_unused]] auto status = do_checkpoint(*trigger);
        }
    }
}

std::optional<CheckpointTrigger> CheckpointManager::should_checkpoint() const {
    auto now = std::chrono::steady_clock::now();
    auto since_last = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_checkpoint_time_
    );
    
    // Get current stats
    std::size_t dirty_count = buffer_pool_->dirty_count();
    std::size_t capacity = buffer_pool_->capacity();
    float dirty_ratio = static_cast<float>(dirty_count) / static_cast<float>(capacity);
    
    std::uint64_t wal_size = wal_->size();
    
    // ==========================================================================
    // Priority 1: Hard limit (CRITICAL - blocks transactions)
    // ==========================================================================
    
    if (dirty_ratio >= config_.dirty_hard_limit) {
        return CheckpointTrigger::DirtyHard;
    }
    
    // ==========================================================================
    // Priority 2: Minimum interval check (prevent checkpoint storm)
    // ==========================================================================
    
    if (since_last < config_.min_interval) {
        return std::nullopt;
    }
    
    // ==========================================================================
    // Priority 3: WAL size
    // ==========================================================================
    
    if (wal_size >= config_.max_wal_size) {
        return CheckpointTrigger::WalSize;
    }
    
    // ==========================================================================
    // Priority 4: Soft limit (background flush)
    // ==========================================================================
    
    if (dirty_ratio >= config_.dirty_soft_limit) {
        return CheckpointTrigger::DirtySoft;
    }
    
    // ==========================================================================
    // Priority 5: Timer
    // ==========================================================================
    
    if (since_last >= config_.max_interval) {
        return CheckpointTrigger::Timer;
    }
    
    return std::nullopt;
}

Status CheckpointManager::do_checkpoint(CheckpointTrigger trigger) {
    auto start_time = std::chrono::steady_clock::now();
    
    checkpoint_in_progress_.store(true, std::memory_order_release);
    
    // If hard limit - block new transactions
    bool is_blocking = (trigger == CheckpointTrigger::DirtyHard);
    if (is_blocking) {
        blocking_.store(true, std::memory_order_release);
        
        if (metrics_) {
            metrics_->blocking_count.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    // ==========================================================================
    // Phase 1: Write CHECKPOINT_BEGIN to WAL
    // ==========================================================================
    
    auto begin_lsn_result = wal_->write_checkpoint_begin();
    if (!begin_lsn_result) {
        checkpoint_in_progress_.store(false, std::memory_order_release);
        blocking_.store(false, std::memory_order_release);
        block_cv_.notify_all();
        return Err(begin_lsn_result.error().code(), begin_lsn_result.error().message());
    }
    
    Lsn begin_lsn = *begin_lsn_result;
    
    // ==========================================================================
    // Phase 2: Get dirty pages snapshot
    // ==========================================================================
    
    auto dirty_pages = buffer_pool_->get_dirty_pages();
    std::size_t total_pages = dirty_pages.size();
    
    if (total_pages == 0) {
        // No dirty pages - just write end record
        [[maybe_unused]] auto end_result = wal_->write_checkpoint_end(begin_lsn);
        
        checkpoint_in_progress_.store(false, std::memory_order_release);
        blocking_.store(false, std::memory_order_release);
        block_cv_.notify_all();
        
        last_checkpoint_time_ = std::chrono::steady_clock::now();
        return Ok();
    }
    
    // ==========================================================================
    // Phase 3: Flush dirty pages in batches
    // ==========================================================================
    
    std::size_t pages_written = 0;
    
    for (std::size_t i = 0; i < dirty_pages.size(); i += config_.batch_size) {
        if (!running_.load(std::memory_order_relaxed) && 
            trigger != CheckpointTrigger::Shutdown) {
            break;  // Interrupted
        }
        
        std::size_t batch_end = std::min(i + config_.batch_size, dirty_pages.size());
        std::vector<PageId> batch(dirty_pages.begin() + static_cast<std::ptrdiff_t>(i),
                                   dirty_pages.begin() + static_cast<std::ptrdiff_t>(batch_end));
        
        if (auto status = buffer_pool_->flush_pages(batch); !status) {
            // Log error but continue with other pages
            continue;
        }
        
        pages_written += batch.size();
        
        // Throttle for soft limit to reduce I/O pressure
        if (trigger == CheckpointTrigger::DirtySoft) {
            std::this_thread::sleep_for(config_.batch_throttle);
        }
    }
    
    // ==========================================================================
    // Phase 4: Sync
    // ==========================================================================
    
    if (auto status = buffer_pool_->sync(); !status) {
        checkpoint_in_progress_.store(false, std::memory_order_release);
        blocking_.store(false, std::memory_order_release);
        block_cv_.notify_all();
        return status;
    }
    
    // ==========================================================================
    // Phase 5: Write CHECKPOINT_END to WAL
    // ==========================================================================
    
    auto end_lsn_result = wal_->write_checkpoint_end(begin_lsn);
    if (!end_lsn_result) {
        checkpoint_in_progress_.store(false, std::memory_order_release);
        blocking_.store(false, std::memory_order_release);
        block_cv_.notify_all();
        return Err(end_lsn_result.error().code(), end_lsn_result.error().message());
    }
    
    // ==========================================================================
    // Phase 6: Truncate old WAL segments
    // ==========================================================================
    
    [[maybe_unused]] auto truncate_status = wal_->truncate(begin_lsn);
    
    // ==========================================================================
    // Finish
    // ==========================================================================
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Record metrics
    bool was_forced = (trigger != CheckpointTrigger::Timer && 
                       trigger != CheckpointTrigger::Manual);
    
    if (metrics_) {
        metrics_->record(duration, pages_written, was_forced, is_blocking);
    }
    
    checkpoint_in_progress_.store(false, std::memory_order_release);
    blocking_.store(false, std::memory_order_release);
    block_cv_.notify_all();
    
    last_checkpoint_time_ = end_time;
    
    return Ok();
}

} // namespace datyredb::storage
