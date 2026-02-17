// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Disk Manager                                                     ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#pragma once

#include "types.hpp"
#include "page.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <atomic>
#include <memory>

namespace datyredb::storage {

/// Manages disk I/O for database pages
class DiskManager {
public:
    // ==========================================================================
    // Construction
    // ==========================================================================
    
    explicit DiskManager(std::filesystem::path data_dir);
    ~DiskManager();
    
    // Non-copyable, non-movable (owns file handles)
    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;
    DiskManager(DiskManager&&) = delete;
    DiskManager& operator=(DiskManager&&) = delete;
    
    // ==========================================================================
    // Lifecycle
    // ==========================================================================
    
    /// Open or create database files
    [[nodiscard]] Status open();
    
    /// Close all files
    void close();
    
    /// Check if manager is open
    [[nodiscard]] bool is_open() const noexcept { return is_open_; }
    
    // ==========================================================================
    // Page I/O
    // ==========================================================================
    
    /// Read page from disk
    [[nodiscard]] Status read_page(PageId page_id, Page& page);
    
    /// Write page to disk
    [[nodiscard]] Status write_page(PageId page_id, const Page& page);
    
    // ==========================================================================
    // Page allocation
    // ==========================================================================
    
    /// Allocate new page and return its ID
    [[nodiscard]] Result<PageId> allocate_page();
    
    /// Deallocate page (add to free list)
    [[nodiscard]] Status deallocate_page(PageId page_id);
    
    // ==========================================================================
    // Sync
    // ==========================================================================
    
    /// Sync all pending writes to disk
    [[nodiscard]] Status sync();
    
    // ==========================================================================
    // Stats
    // ==========================================================================
    
    /// Number of allocated pages
    [[nodiscard]] PageId page_count() const noexcept {
        return next_page_id_.load(std::memory_order_relaxed);
    }
    
    /// Total data file size in bytes
    [[nodiscard]] std::uint64_t file_size() const noexcept {
        return static_cast<std::uint64_t>(page_count()) * PAGE_SIZE;
    }
    
    /// Data directory path
    [[nodiscard]] const std::filesystem::path& data_dir() const noexcept {
        return data_dir_;
    }
    
private:
    /// Ensure file is large enough for given page
    [[nodiscard]] Status ensure_file_size(PageId page_id);
    
    std::filesystem::path data_dir_;
    std::filesystem::path data_file_path_;
    std::fstream data_file_;
    
    std::mutex io_mutex_;
    std::atomic<PageId> next_page_id_{0};
    std::atomic<bool> is_open_{false};
};

} // namespace datyredb::storage
