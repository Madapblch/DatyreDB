#pragma once

#include "storage/storage_types.hpp"
#include "storage/page.hpp"

#include <string>
#include <fstream>
#include <mutex>
#include <filesystem>
#include <atomic>

namespace datyredb::storage {

/// Управление дисковым I/O
class DiskManager {
public:
    explicit DiskManager(const std::filesystem::path& db_path);
    ~DiskManager();
    
    // Запретить копирование
    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;
    
    /// Инициализация
    bool initialize();
    
    /// Закрытие
    void shutdown();
    
    /// Чтение страницы с диска
    bool read_page(PageId page_id, Page& page);
    
    /// Запись страницы на диск
    bool write_page(PageId page_id, const Page& page);
    
    /// Выделение новой страницы
    PageId allocate_page();
    
    /// Освобождение страницы
    void deallocate_page(PageId page_id);
    
    /// fsync
    void sync();
    
    /// Размер файла данных
    uint64_t data_file_size() const;
    
    /// Количество страниц
    PageId page_count() const { 
        return next_page_id_.load(std::memory_order_relaxed); 
    }
    
    /// Путь к данным
    const std::filesystem::path& data_path() const { return db_path_; }
    
private:
    std::filesystem::path db_path_;
    std::filesystem::path data_file_path_;
    std::fstream data_file_;
    std::mutex io_mutex_;
    std::atomic<PageId> next_page_id_{0};
    bool initialized_ = false;
};

} // namespace datyredb::storage
