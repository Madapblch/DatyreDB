// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Checkpoint Manager                                               ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#pragma once

#include "types.hpp"
#include "buffer_pool.hpp"
#include "wal.hpp"

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>

namespace datyredb::storage {

/// Checkpoint Manager with adaptive triggers and memory pressure handling
class CheckpointManager {
public:
    // ==========================================================================
    // Construction
    // ==========================================================================
    
    CheckpointManager(
        CheckpointConfig config,
        std::shared_ptr<BufferPool> buffer_pool,
        std::shared_ptr<WriteAheadLog> wal,
        std::shared_ptr<CheckpointMetrics> metrics
    );
    
    ~CheckpointManager();
    
    // Non-copyable, non-movable
    CheckpointManager(const CheckpointManager&) = delete;
    CheckpointManager& operator=(const CheckpointManager&) = delete;
    CheckpointManager(CheckpointManager&&) = delete;
    CheckpointManager& operator=(CheckpointManager&&) = delete;
    
    // ==========================================================================
    // Lifecycle
    // ==========================================================================
    
    /// Start background checkpoint thread
    void start();
    
    /// Stop and wait for background thread
    void stop();
    
    /// Check if running
    [[nodiscard]] bool is_running() const noexcept {
        return running_.load(std::memory_order_relaxed);
    }
    
    // ==========================================================================
    // Manual Operations
    // ==========================================================================
    
    /// Request immediate checkpoint
    void request_checkpoint();
    
    /// Perform synchronous checkpoint (blocks until complete)
    [[nodiscard]] Status checkpoint_sync();
    
    // ==========================================================================
    // Pressure Handling
    // ==========================================================================
    
    /// Check memory pressure and potentially block
    /// Returns true if caller had to wait
    [[nodiscard]] bool check_pressure();
    
private:
    // ==========================================================================
    // Internal Types
    // ==========================================================================
    
    /// Background thread main loop
    void background_loop();
    
    /// Check if checkpoint should be triggered
    [[nodiscard]] std::optional<CheckpointTrigger> should_checkpoint() const;
    
    /// Perform checkpoint with given trigger
    [[nodiscard]] Status do_checkpoint(CheckpointTrigger trigger);
    
    // ==========================================================================
    // Data Members
    // ==========================================================================
    
    CheckpointConfig config_;
    std::shared_ptr<BufferPool> buffer_pool_;
    std::shared_ptr<WriteAheadLog> wal_;
    std::shared_ptr<CheckpointMetrics> metrics_;
    
    std::thread background_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> checkpoint_requested_{false};
    std::atomic<bool> checkpoint_in_progress_{false};
    
    // For blocking under hard limit
    std::mutex block_mutex_;
    std::condition_variable block_cv_;
    std::atomic<bool> blocking_{false};
    
    // For background thread wakeup
    std::mutex wake_mutex_;
    std::condition_variable wake_cv_;
    
    std::chrono::steady_clock::time_point last_checkpoint_time_;
    
    std::mutex checkpoint_mutex_;
};

} // namespace datyredb::storage
