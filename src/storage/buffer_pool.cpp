#include "storage/buffer_pool.hpp"
#include "utils/logger.hpp"

namespace datyredb::storage {

BufferPool::BufferPool(std::size_t pool_size,
                       std::shared_ptr<DiskManager> disk_manager,
                       std::shared_ptr<CheckpointMetrics> metrics)
    : pool_size_(pool_size)
    , disk_manager_(std::move(disk_manager))
    , metrics_(std::move(metrics))
    , frames_(pool_size)
{
    // Инициализация free list
    for (std::size_t i = 0; i < pool_size_; ++i) {
        free_list_.push_back(i);
    }
    
    Logger::info("BufferPool initialized: {} frames ({} MB)",
                 pool_size_, 
                 (pool_size_ * PAGE_SIZE) / (1024 * 1024));
}

BufferPool::~BufferPool() {
    // Flush все dirty pages при shutdown
    auto dirty = get_dirty_pages();
    if (!dirty.empty()) {
        Logger::info("BufferPool shutdown: flushing {} dirty pages", dirty.size());
        flush_pages(dirty);
        sync_all();
    }
}

Page* BufferPool::fetch_page(PageId page_id) {
    std::unique_lock lock(latch_);
    
    // Проверяем, есть ли страница в pool
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        auto& frame = frames_[it->second];
        frame.page.pin();
        frame.referenced = true;  // Для Clock-Sweep
        return &frame.page;
    }
    
    // Нужно загрузить с диска — ищем victim frame
    Frame* frame = find_victim_frame();
    if (!frame) {
        Logger::error("BufferPool: no available frames (all pinned)");
        return nullptr;
    }
    
    // Читаем с диска
    if (!disk_manager_->read_page(page_id, frame->page)) {
        Logger::error("BufferPool: failed to read page {}", page_id);
        // Возвращаем frame в free list
        std::size_t idx = frame - frames_.data();
        free_list_.push_back(idx);
        return nullptr;
    }
    
    frame->page.pin();
    frame->page.mark_clean();
    frame->referenced = true;
    
    // Обновляем page table
    std::size_t frame_idx = frame - frames_.data();
    page_table_[page_id] = frame_idx;
    
    return &frame->page;
}

Page* BufferPool::new_page(PageId* out_page_id) {
    std::unique_lock lock(latch_);
    
    Frame* frame = find_victim_frame();
    if (!frame) {
        Logger::error("BufferPool: no available frames for new page");
        return nullptr;
    }
    
    PageId new_id = disk_manager_->allocate_page();
    
    frame->page.reset();
    frame->page.set_page_id(new_id);
    frame->page.pin();
    frame->page.mark_clean();
    frame->referenced = true;
    
    std::size_t frame_idx = frame - frames_.data();
    page_table_[new_id] = frame_idx;
    
    if (out_page_id) {
        *out_page_id = new_id;
    }
    
    Logger::debug("BufferPool: created new page {}", new_id);
    return &frame->page;
}

bool BufferPool::unpin_page(PageId page_id, bool is_dirty) {
    std::unique_lock lock(latch_);
    
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        Logger::warn("BufferPool: unpin on non-existent page {}", page_id);
        return false;
    }
    
    auto& frame = frames_[it->second];
    
    if (frame.page.pin_count() <= 0) {
        Logger::warn("BufferPool: unpin on page {} with pin_count=0", page_id);
        return false;
    }
    
    frame.page.unpin();
    
    // Отмечаем dirty если нужно
    if (is_dirty && !frame.page.is_dirty()) {
        frame.page.mark_dirty();
        std::size_t new_count = dirty_count_.fetch_add(1, std::memory_order_relaxed) + 1;
        metrics_->dirty_page_count.store(new_count, std::memory_order_relaxed);
    }
    
    return true;
}

bool BufferPool::flush_page(PageId page_id) {
    std::unique_lock lock(latch_);
    
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return true;  // Страницы нет в pool — уже на диске
    }
    
    auto& frame = frames_[it->second];
    
    if (!frame.page.is_dirty()) {
        return true;  // Не dirty — не нужно flush
    }
    
    if (!disk_manager_->write_page(page_id, frame.page)) {
        Logger::error("BufferPool: failed to flush page {}", page_id);
        return false;
    }
    
    frame.page.mark_clean();
    std::size_t new_count = dirty_count_.fetch_sub(1, std::memory_order_relaxed) - 1;
    metrics_->dirty_page_count.store(new_count, std::memory_order_relaxed);
    
    return true;
}

bool BufferPool::delete_page(PageId page_id) {
    std::unique_lock lock(latch_);
    
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return true;  // Уже удалена
    }
    
    auto& frame = frames_[it->second];
    
    if (frame.page.is_pinned()) {
        Logger::error("BufferPool: cannot delete pinned page {}", page_id);
        return false;
    }
    
    if (frame.page.is_dirty()) {
        dirty_count_.fetch_sub(1, std::memory_order_relaxed);
        metrics_->dirty_page_count.fetch_sub(1, std::memory_order_relaxed);
    }
    
    std::size_t frame_idx = it->second;
    page_table_.erase(it);
    free_list_.push_back(frame_idx);
    frame.page.reset();
    
    disk_manager_->deallocate_page(page_id);
    
    return true;
}

std::vector<PageId> BufferPool::get_dirty_pages() const {
    std::shared_lock lock(latch_);
    
    std::vector<PageId> result;
    result.reserve(dirty_count_.load(std::memory_order_relaxed));
    
    for (const auto& [page_id, frame_idx] : page_table_) {
        if (frames_[frame_idx].page.is_dirty()) {
            result.push_back(page_id);
        }
    }
    
    return result;
}

bool BufferPool::flush_pages(const std::vector<PageId>& pages) {
    bool success = true;
    
    for (PageId page_id : pages) {
        if (!flush_page(page_id)) {
            Logger::error("BufferPool: failed to flush page {}", page_id);
            success = false;
        }
    }
    
    return success;
}

void BufferPool::sync_all() {
    disk_manager_->sync();
}

std::size_t BufferPool::page_count() const {
    std::shared_lock lock(latch_);
    return page_table_.size();
}

BufferPool::Frame* BufferPool::find_victim_frame() {
    // Сначала проверяем free list
    if (!free_list_.empty()) {
        std::size_t idx = free_list_.front();
        free_list_.pop_front();
        return &frames_[idx];
    }
    
    // Clock-Sweep eviction
    return clock_sweep();
}

BufferPool::Frame* BufferPool::clock_sweep() {
    // Два прохода: первый сбрасывает reference bit
    for (int pass = 0; pass < 2; ++pass) {
        for (std::size_t i = 0; i < pool_size_; ++i) {
            std::size_t idx = (clock_hand_ + i) % pool_size_;
            auto& frame = frames_[idx];
            
            // Пропускаем pinned
            if (frame.page.is_pinned()) {
                continue;
            }
            
            // Пропускаем свободные
            if (frame.page.page_id() == INVALID_PAGE_ID) {
                continue;
            }
            
            if (frame.referenced) {
                // Сбрасываем reference bit — даём второй шанс
                frame.referenced = false;
                continue;
            }
            
            // Нашли victim!
            if (!evict_frame(&frame)) {
                continue;  // Не удалось evict — ищем дальше
            }
            
            clock_hand_ = (idx + 1) % pool_size_;
            return &frame;
        }
    }
    
    // Все страницы pinned или referenced
    return nullptr;
}

bool BufferPool::evict_frame(Frame* frame) {
    PageId page_id = frame->page.page_id();
    
    // Если dirty — сначала flush
    if (frame->page.is_dirty()) {
        if (!disk_manager_->write_page(page_id, frame->page)) {
            Logger::error("BufferPool: failed to evict dirty page {}", page_id);
            return false;
        }
        dirty_count_.fetch_sub(1, std::memory_order_relaxed);
        metrics_->dirty_page_count.fetch_sub(1, std::memory_order_relaxed);
    }
    
    // Удаляем из page table
    page_table_.erase(page_id);
    frame->page.reset();
    frame->referenced = false;
    
    return true;
}

} // namespace datyredb::storage
