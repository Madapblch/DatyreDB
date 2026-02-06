#pragma once

#include "../common/types.h"
#include "../storage/disk_manager.h"
#include <unordered_map>
#include <memory>
#include <list> // For LRU eviction

class BufferPool {
public:
    explicit BufferPool(DiskManager* disk_manager, std::size_t pool_size = 128);

    // Вернуть указатель на страницу в памяти (или nullptr при ошибке)
    Page* GetPage(page_id_t page_id);

    // Помечает страницу как грязную (для future async flush)
    void MarkDirty(page_id_t page_id);

    // Инвалидирует страницу в кэше (удаляет, чтобы перезагрузить с диска)
    void Invalidate(page_id_t page_id);

private:
    DiskManager* disk_manager_;
    std::size_t pool_size_;
    std::unordered_map<page_id_t, std::unique_ptr<Page>> pages_;
    std::list<page_id_t> lru_list_; // LRU for eviction
    std::unordered_map<page_id_t, std::list<page_id_t>::iterator> lru_map_;
};