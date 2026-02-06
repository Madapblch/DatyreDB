#include "buffer_pool.h"

#include <iostream>
#include <algorithm> // for std::find_if or random

BufferPool::BufferPool(DiskManager* disk_manager, std::size_t pool_size)
    : disk_manager_(disk_manager), pool_size_(pool_size) {}

Page* BufferPool::GetPage(page_id_t page_id) {
    // 1. Ищем в кэше
    auto it = pages_.find(page_id);
    if (it != pages_.end()) {
        // Update LRU: move to front
        lru_list_.erase(lru_map_[page_id]);
        lru_list_.push_front(page_id);
        lru_map_[page_id] = lru_list_.begin();
        return it->second.get();
    }

    // 2. Если нет в кэше — загружаем с диска
    if (pages_.size() >= pool_size_) {
        // Evict LRU page (least recently used)
        page_id_t evict_id = lru_list_.back();
        lru_list_.pop_back();
        lru_map_.erase(evict_id);
        pages_.erase(evict_id);
    }

    auto new_page = std::make_unique<Page>();
    Status s = disk_manager_->ReadPage(page_id, *new_page);
    if (s != Status::OK) {
        std::cerr << "ReadPage failed for page " << page_id << "\n";
        return nullptr;
    }

    Page* ptr = new_page.get();
    pages_[page_id] = std::move(new_page);
    // Add to LRU front
    lru_list_.push_front(page_id);
    lru_map_[page_id] = lru_list_.begin();
    return ptr;
}

void BufferPool::MarkDirty(page_id_t /*page_id*/) {
    // Пока ничего не делаем (future: async flush dirty pages)
}

void BufferPool::Invalidate(page_id_t page_id) {
    auto it = pages_.find(page_id);
    if (it != pages_.end()) {
        pages_.erase(it);
        auto lru_it = lru_map_.find(page_id);
        if (lru_it != lru_map_.end()) {
            lru_list_.erase(lru_it->second);
            lru_map_.erase(lru_it);
        }
    }
}