// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Buffer Pool Manager                                              ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#pragma once

#include "types.hpp"
#include "page.hpp"
#include "disk_manager.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <list>
#include <shared_mutex>
#include <atomic>
#include <optional>
#include <functional>

namespace datyredb::storage {

/// Buffer pool frame containing a page and metadata
struct Frame {
    Page page;
    bool referenced{false};  // For Clock-Sweep algorithm
    
    Frame() = default;
    ~Frame() = default;
    
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    Frame(Frame&&) = default;
    Frame& operator=(Frame&&) = default;
};

/// Buffer Pool Manager with Clock-Sweep eviction and dirty page tracking
class BufferPool {
public:
    // ==========================================================================
    // Construction
    // ==========================================================================
    
    BufferPool(
        std::size_t pool_size,
        std::shared_ptr<DiskManager> disk_manager,
        std::shared_ptr<CheckpointMetrics> metrics
    );
    
    ~BufferPool();
    
    // Non-copyable, non-movable
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    BufferPool(BufferPool&&) = delete;
    BufferPool& operator=(BufferPool&&) = delete;
    
    // ==========================================================================
    // Page Access
    // ==========================================================================
    
    /// Fetch page from pool (loads from disk if necessary)
    /// Returns pinned page - caller MUST call unpin_page when done
    [[nodiscard]] Result<Page*> fetch_page(PageId page_id);
    
    /// Create and return a new page
    /// Returns pinned page - caller MUST call unpin_page when done
    [[nodiscard]] Result<Page*> new_page();
    
    /// Unpin page after use
    /// @param is_dirty Set true if page was modified
    [[nodiscard]] Status unpin_page(PageId page_id, bool is_dirty);
    
    /// Flush specific page to disk
    [[nodiscard]] Status flush_page(PageId page_id);
    
    /// Delete page from pool and disk
    [[nodiscard]] Status delete_page(PageId page_id);
    
    // ==========================================================================
    // Bulk Operations (for checkpoint)
    // ==========================================================================
    
    /// Get list of all dirty page IDs (snapshot)
    [[nodiscard]] std::vector<PageId> get_dirty_pages() const;
    
    /// Flush multiple pages to disk
    [[nodiscard]] Status flush_pages(const std::vector<PageId>& page_ids);
    
    /// Sync all underlying files
    [[nodiscard]] Status sync();
    
    // ==========================================================================
    // Statistics
    // ==========================================================================
    
    /// Total pool capacity (number of frames)
    [[nodiscard]] std::size_t capacity() const noexcept { return pool_size_; }
    
    /// Number of pages currently in pool
    [[nodiscard]] std::size_t size() const noexcept;
    
    /// Number of dirty pages
    [[nodiscard]] std::size_t dirty_count() const noexcept {
        return dirty_count_.load(std::memory_order_relaxed);
    }
    
    /// Number of pinned pages
    [[nodiscard]] std::size_t pinned_count() const noexcept;
    
    // ==========================================================================
    // Iteration (for debugging/monitoring)
    // ==========================================================================
    
    /// Execute function for each page in pool
    void for_each_page(std::function<void(PageId, const Page&)> fn) const;
    
private:
    // ==========================================================================
    // Internal Types
    // ==========================================================================
    
    /// Find a victim frame for eviction
    [[nodiscard]] std::optional<FrameId> find_victim();
    
    /// Clock-Sweep eviction algorithm
    [[nodiscard]] std::optional<FrameId> clock_sweep();
    
    /// Evict page from frame (flush if dirty)
    [[nodiscard]] Status evict_frame(FrameId frame_id);
    
    /// Get frame by ID
    [[nodiscard]] Frame& get_frame(FrameId frame_id) { return frames_[frame_id]; }
    [[nodiscard]] const Frame& get_frame(FrameId frame_id) const { return frames_[frame_id]; }
    
    // ==========================================================================
    // Data Members
    // ==========================================================================
    
    std::size_t pool_size_;
    std::shared_ptr<DiskManager> disk_manager_;
    std::shared_ptr<CheckpointMetrics> metrics_;
    
    /// Frame storage
    std::vector<Frame> frames_;
    
    /// Page ID -> Frame ID mapping
    std::unordered_map<PageId, FrameId> page_table_;
    
    /// Free frame list
    std::list<FrameId> free_list_;
    
    /// Clock hand for eviction
    FrameId clock_hand_{0};
    
    /// Dirty page count (atomic for lock-free reads)
    std::atomic<std::size_t> dirty_count_{0};
    
    /// Main latch for thread safety
    mutable std::shared_mutex latch_;
};

} // namespace datyredb::storage
