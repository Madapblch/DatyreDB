#pragma once

#include "storage/storage_types.hpp"
#include "storage/page.hpp"
#include "storage/disk_manager.hpp"

#include <unordered_map>
#include <list>
#include <vector>
#include <shared_mutex>
#include <memory>
#include <atomic>
#include <optional>

namespace datyredb::storage {

/// Buffer Pool Manager с Clock-Sweep eviction и dirty page tracking
class BufferPool {
public:
    BufferPool(std::size_t pool_size, 
               std::shared_ptr<DiskManager> disk_manager,
               std::shared_ptr<CheckpointMetrics> metrics);
    ~BufferPool();
    
    // Запретить копирование
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    
    // ========================================================================
    // Page access
    // ========================================================================
    
    /// Получить страницу (загружает с диска если нужно)
    Page* fetch_page(PageId page_id);
    
    /// Создать новую страницу
    Page* new_page(PageId* out_page_id = nullptr);
    
    /// Освободить страницу (unpin)
    bool unpin_page(PageId page_id, bool is_dirty);
    
    /// Flush страницы на диск
    bool flush_page(PageId page_id);
    
    /// Удалить страницу
    bool delete_page(PageId page_id);
    
    // ========================================================================
    // Checkpoint support
    // ========================================================================
    
    /// Получить список dirty pages (snapshot для checkpoint)
    std::vector<PageId> get_dirty_pages() const;
    
    /// Flush батча страниц
    bool flush_pages(const std::vector<PageId>& pages);
    
    /// Sync все файлы
    void sync_all();
    
    // ========================================================================
    // Stats
    // ========================================================================
    
    /// Общее количество слотов
    std::size_t capacity() const { return pool_size_; }
    
    /// Текущее количество dirty pages
    std::size_t dirty_page_count() const { 
        return dirty_count_.load(std::memory_order_relaxed); 
    }
    
    /// Текущее количество страниц в pool
    std::size_t page_count() const;
    
private:
    /// Frame в buffer pool
    struct Frame {
        Page page;
        bool referenced = false;  // Для Clock-Sweep
    };
    
    /// Найти свободный frame или evict
    Frame* find_victim_frame();
    
    /// Clock-Sweep eviction
    Frame* clock_sweep();
    
    /// Evict конкретный frame
    bool evict_frame(Frame* frame);
    
    std::size_t pool_size_;
    std::shared_ptr<DiskManager> disk_manager_;
    std::shared_ptr<CheckpointMetrics> metrics_;
    
    // Пул фреймов
    std::vector<Frame> frames_;
    
    // Page ID -> Frame index
    std::unordered_map<PageId, std::size_t> page_table_;
    
    // Список свободных фреймов
    std::list<std::size_t> free_list_;
    
    // Clock hand для eviction
    std::size_t clock_hand_ = 0;
    
    // Dirty page counter
    std::atomic<std::size_t> dirty_count_{0};
    
    mutable std::shared_mutex latch_;
};

} // namespace datyredb::storage
