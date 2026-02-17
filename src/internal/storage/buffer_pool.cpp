// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Buffer Pool Manager Implementation                               ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "buffer_pool.hpp"

#include <fmt/core.h>
#include <algorithm>

namespace datyredb::storage {

BufferPool::BufferPool(
    std::size_t pool_size,
    std::shared_ptr<DiskManager> disk_manager,
    std::shared_ptr<CheckpointMetrics> metrics
)
    : pool_size_(pool_size)
    , disk_manager_(std::move(disk_manager))
    , metrics_(std::move(metrics))
    , frames_(pool_size)
{
    // Initialize free list with all frame IDs
    for (FrameId i = 0; i < static_cast<FrameId>(pool_size); ++i) {
        free_list_.push_back(i);
    }
}

BufferPool::~BufferPool() {
    // Flush all dirty pages on destruction
    auto dirty_pages = get_dirty_pages();
    if (!dirty_pages.empty()) {
        [[maybe_unused]] auto status = flush_pages(dirty_pages);
        [[maybe_unused]] auto sync_status = sync();
    }
}

// ==============================================================================
// Page Access
// ==============================================================================

Result<Page*> BufferPool::fetch_page(PageId page_id) {
    std::unique_lock lock(latch_);
    
    // Check if page is already in pool
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        FrameId frame_id = it->second;
        Frame& frame = get_frame(frame_id);
        
        frame.page.pin();
        frame.referenced = true;
        
        return Ok(&frame.page);
    }
    
    // Page not in pool - need to load from disk
    auto victim_opt = find_victim();
    if (!victim_opt) {
        return Err<Page*>(ErrorCode::NoAvailableFrames, 
            "All frames are pinned, cannot evict");
    }
    
    FrameId frame_id = *victim_opt;
    Frame& frame = get_frame(frame_id);
    
    // Load page from disk
    if (auto status = disk_manager_->read_page(page_id, frame.page); !status) {
        // Return frame to free list
        free_list_.push_back(frame_id);
        return Err<Page*>(status.error().code(), status.error().message());
    }
    
    // Setup frame
    frame.page.pin();
    frame.page.mark_clean();
    frame.referenced = true;
    
    // Update page table
    page_table_[page_id] = frame_id;
    
    return Ok(&frame.page);
}

Result<Page*> BufferPool::new_page() {
    std::unique_lock lock(latch_);
    
    // Allocate page on disk
    auto page_id_result = disk_manager_->allocate_page();
    if (!page_id_result) {
        return Err<Page*>(page_id_result.error().code(), 
            page_id_result.error().message());
    }
    
    PageId page_id = page_id_result.value();
    
    // Find frame for new page
    auto victim_opt = find_victim();
    if (!victim_opt) {
        return Err<Page*>(ErrorCode::NoAvailableFrames,
            "All frames are pinned, cannot create new page");
    }
    
    FrameId frame_id = *victim_opt;
    Frame& frame = get_frame(frame_id);
    
    // Initialize new page
    frame.page.reset(page_id);
    frame.page.pin();
    frame.page.mark_clean();
    frame.referenced = true;
    
    // Update page table
    page_table_[page_id] = frame_id;
    
    return Ok(&frame.page);
}

Status BufferPool::unpin_page(PageId page_id, bool is_dirty) {
    std::unique_lock lock(latch_);
    
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return Err(ErrorCode::PageNotFound,
            fmt::format("Page {} not in buffer pool", page_id));
    }
    
    FrameId frame_id = it->second;
    Frame& frame = get_frame(frame_id);
    
    if (frame.page.pin_count() <= 0) {
        return Err(ErrorCode::InvalidArgument,
            fmt::format("Page {} has pin_count <= 0", page_id));
    }
    
    frame.page.unpin();
    
    // Mark dirty if requested and not already dirty
    if (is_dirty && !frame.page.is_dirty()) {
        frame.page.mark_dirty();
        auto new_count = dirty_count_.fetch_add(1, std::memory_order_relaxed) + 1;
        
        if (metrics_) {
            // Исправлено: теперь поле существует в types.hpp
            metrics_->dirty_pages.store(new_count, std::memory_order_relaxed);
        }
    }
    
    return Ok();
}

Status BufferPool::flush_page(PageId page_id) {
    std::unique_lock lock(latch_);
    
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        // Page not in pool - nothing to flush
        return Ok();
    }
    
    FrameId frame_id = it->second;
    Frame& frame = get_frame(frame_id);
    
    if (!frame.page.is_dirty()) {
        // Not dirty - nothing to flush
        return Ok();
    }
    
    // Write to disk
    if (auto status = disk_manager_->write_page(page_id, frame.page); !status) {
        return status;
    }
    
    // Mark clean
    frame.page.mark_clean();
    auto new_count = dirty_count_.fetch_sub(1, std::memory_order_relaxed) - 1;
    
    if (metrics_) {
        metrics_->dirty_pages.store(new_count, std::memory_order_relaxed);
    }
    
    return Ok();
}

Status BufferPool::delete_page(PageId page_id) {
    std::unique_lock lock(latch_);
    
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        // Page not in pool
        return disk_manager_->deallocate_page(page_id);
    }
    
    FrameId frame_id = it->second;
    Frame& frame = get_frame(frame_id);
    
    if (frame.page.is_pinned()) {
        return Err(ErrorCode::PagePinned,
            fmt::format("Cannot delete pinned page {}", page_id));
    }
    
    // Update dirty count if necessary
    if (frame.page.is_dirty()) {
        dirty_count_.fetch_sub(1, std::memory_order_relaxed);
        if (metrics_) {
            metrics_->dirty_pages.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    
    // Remove from page table and add to free list
    page_table_.erase(it);
    frame.page.reset();
    frame.referenced = false;
    free_list_.push_back(frame_id);
    
    return disk_manager_->deallocate_page(page_id);
}

// ==============================================================================
// Bulk Operations
// ==============================================================================

std::vector<PageId> BufferPool::get_dirty_pages() const {
    std::shared_lock lock(latch_);
    
    std::vector<PageId> dirty_pages;
    dirty_pages.reserve(dirty_count_.load(std::memory_order_relaxed));
    
    for (const auto& [page_id, frame_id] : page_table_) {
        if (get_frame(frame_id).page.is_dirty()) {
            dirty_pages.push_back(page_id);
        }
    }
    
    return dirty_pages;
}

Status BufferPool::flush_pages(const std::vector<PageId>& page_ids) {
    for (PageId page_id : page_ids) {
        if (auto status = flush_page(page_id); !status) {
            return status;
        }
    }
    return Ok();
}

Status BufferPool::sync() {
    return disk_manager_->sync();
}

// ==============================================================================
// Statistics
// ==============================================================================

std::size_t BufferPool::size() const noexcept {
    std::shared_lock lock(latch_);
    return page_table_.size();
}

std::size_t BufferPool::pinned_count() const noexcept {
    std::shared_lock lock(latch_);
    
    std::size_t count = 0;
    for (const auto& [page_id, frame_id] : page_table_) {
        if (get_frame(frame_id).page.is_pinned()) {
            ++count;
        }
    }
    return count;
}

// ==============================================================================
// Iteration
// ==============================================================================

void BufferPool::for_each_page(std::function<void(PageId, const Page&)> fn) const {
    std::shared_lock lock(latch_);
    
    for (const auto& [page_id, frame_id] : page_table_) {
        fn(page_id, get_frame(frame_id).page);
    }
}

// ==============================================================================
// Internal Methods
// ==============================================================================

std::optional<FrameId> BufferPool::find_victim() {
    // First check free list
    if (!free_list_.empty()) {
        FrameId frame_id = free_list_.front();
        free_list_.pop_front();
        return frame_id;
    }
    
    // Use Clock-Sweep to find victim
    return clock_sweep();
}

std::optional<FrameId> BufferPool::clock_sweep() {
    // Two passes: first pass clears reference bits, second pass finds victim
    for (int pass = 0; pass < 2; ++pass) {
        for (std::size_t i = 0; i < pool_size_; ++i) {
            // Explicit cast to avoid conversion warning
            std::size_t next_idx = (static_cast<std::size_t>(clock_hand_) + i) % pool_size_;
            FrameId frame_id = static_cast<FrameId>(next_idx);
            
            Frame& frame = get_frame(frame_id);
            
            // Skip if not in use
            if (frame.page.id() == INVALID_PAGE_ID) {
                continue;
            }
            
            // Skip if pinned
            if (frame.page.is_pinned()) {
                continue;
            }
            
            // Check reference bit
            if (frame.referenced) {
                // Give second chance
                frame.referenced = false;
                continue;
            }
            
            // Found victim - evict it
            if (auto status = evict_frame(frame_id); !status) {
                continue;  // Try next frame
            }
            
            // Update clock hand
            clock_hand_ = static_cast<FrameId>((next_idx + 1) % pool_size_);
            
            return frame_id;
        }
    }
    
    // All frames are pinned
    return std::nullopt;
}

Status BufferPool::evict_frame(FrameId frame_id) {
    Frame& frame = get_frame(frame_id);
    PageId page_id = frame.page.id();
    
    // Flush if dirty
    if (frame.page.is_dirty()) {
        if (auto status = disk_manager_->write_page(page_id, frame.page); !status) {
            return status;
        }
        
        dirty_count_.fetch_sub(1, std::memory_order_relaxed);
        if (metrics_) {
            metrics_->dirty_pages.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    
    // Remove from page table
    page_table_.erase(page_id);
    
    // Reset frame
    frame.page.reset();
    frame.referenced = false;
    
    return Ok();
}

} // namespace datyredb::storage
