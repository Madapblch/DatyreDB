// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Disk Manager Implementation                                      ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "disk_manager.hpp"

#include <fmt/core.h>

namespace datyredb::storage {

DiskManager::DiskManager(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir))
    , data_file_path_(data_dir_ / "data.db")
{
}

DiskManager::~DiskManager() {
    close();
}

Status DiskManager::open() {
    if (is_open_) {
        return Ok();
    }
    
    std::lock_guard lock(io_mutex_);
    
    // Create directory if needed
    std::error_code ec;
    std::filesystem::create_directories(data_dir_, ec);
    if (ec) {
        return Err(ErrorCode::IoError, 
            fmt::format("Failed to create directory '{}': {}", 
                data_dir_.string(), ec.message()));
    }
    
    // Open or create data file
    data_file_.open(data_file_path_, 
        std::ios::in | std::ios::out | std::ios::binary);
    
    if (!data_file_.is_open()) {
        // File doesn't exist - create it
        data_file_.open(data_file_path_, 
            std::ios::out | std::ios::binary);
        
        if (!data_file_.is_open()) {
            return Err(ErrorCode::IoError,
                fmt::format("Failed to create file '{}'", data_file_path_.string()));
        }
        
        data_file_.close();
        data_file_.open(data_file_path_,
            std::ios::in | std::ios::out | std::ios::binary);
    }
    
    if (!data_file_.is_open()) {
        return Err(ErrorCode::IoError,
            fmt::format("Failed to open file '{}'", data_file_path_.string()));
    }
    
    // Determine current page count
    data_file_.seekg(0, std::ios::end);
    auto file_size = static_cast<std::uint64_t>(data_file_.tellg());
    next_page_id_.store(static_cast<PageId>(file_size / PAGE_SIZE), std::memory_order_relaxed);
    
    is_open_ = true;
    return Ok();
}

void DiskManager::close() {
    if (!is_open_) {
        return;
    }
    
    std::lock_guard lock(io_mutex_);
    
    if (data_file_.is_open()) {
        data_file_.flush();
        data_file_.close();
    }
    
    is_open_ = false;
}

Status DiskManager::read_page(PageId page_id, Page& page) {
    if (!is_open_) {
        return Err(ErrorCode::IoError, "Disk manager not open");
    }
    
    if (page_id >= next_page_id_.load(std::memory_order_relaxed)) {
        return Err(ErrorCode::InvalidPageId,
            fmt::format("Page ID {} out of range (max: {})", 
                page_id, next_page_id_.load() - 1));
    }
    
    std::lock_guard lock(io_mutex_);
    
    // Explicit cast to avoid warnings
    auto offset = static_cast<std::streamoff>(static_cast<std::uint64_t>(page_id) * PAGE_SIZE);
    data_file_.seekg(offset, std::ios::beg);
    
    if (!data_file_) {
        return Err(ErrorCode::ReadError,
            fmt::format("Failed to seek to page {}", page_id));
    }
    
    data_file_.read(page.data(), PAGE_SIZE);
    
    if (!data_file_) {
        return Err(ErrorCode::ReadError,
            fmt::format("Failed to read page {}", page_id));
    }
    
    page.set_id(page_id);
    page.mark_clean();
    
    // Verify checksum
    if (!page.verify_checksum()) {
        return Err(ErrorCode::ChecksumMismatch,
            fmt::format("Checksum mismatch for page {}", page_id));
    }
    
    return Ok();
}

Status DiskManager::write_page(PageId page_id, const Page& page) {
    if (!is_open_) {
        return Err(ErrorCode::IoError, "Disk manager not open");
    }
    
    std::lock_guard lock(io_mutex_);
    
    // Ensure file is large enough
    if (auto status = ensure_file_size(page_id); !status) {
        return status;
    }
    
    // Update checksum before writing
    auto& mutable_page = const_cast<Page&>(page);
    mutable_page.update_checksum();
    
    // Explicit cast to avoid warnings
    auto offset = static_cast<std::streamoff>(static_cast<std::uint64_t>(page_id) * PAGE_SIZE);
    data_file_.seekp(offset, std::ios::beg);
    
    if (!data_file_) {
        return Err(ErrorCode::WriteError,
            fmt::format("Failed to seek for writing page {}", page_id));
    }
    
    data_file_.write(page.data(), PAGE_SIZE);
    
    if (!data_file_) {
        return Err(ErrorCode::WriteError,
            fmt::format("Failed to write page {}", page_id));
    }
    
    return Ok();
}

Result<PageId> DiskManager::allocate_page() {
    if (!is_open_) {
        return Err<PageId>(ErrorCode::IoError, "Disk manager not open");
    }
    
    PageId new_id = next_page_id_.fetch_add(1, std::memory_order_relaxed);
    
    // Extend file
    {
        std::lock_guard lock(io_mutex_);
        
        if (auto status = ensure_file_size(new_id); !status) {
            // Rollback
            next_page_id_.fetch_sub(1, std::memory_order_relaxed);
            return Err<PageId>(status.error().code(), status.error().message());
        }
    }
    
    return Ok(new_id);
}

Status DiskManager::deallocate_page(PageId page_id) {
    // TODO: Implement free list
    // For now, pages are never truly freed
    (void)page_id;
    return Ok();
}

Status DiskManager::sync() {
    if (!is_open_) {
        return Err(ErrorCode::IoError, "Disk manager not open");
    }
    
    std::lock_guard lock(io_mutex_);
    
    data_file_.flush();
    
    if (!data_file_) {
        return Err(ErrorCode::SyncError, "Failed to sync data file");
    }
    
    return Ok();
}

Status DiskManager::ensure_file_size(PageId page_id) {
    // Explicit casts to avoid warnings
    auto required_size = static_cast<std::streamoff>(
        (static_cast<std::uint64_t>(page_id) + 1) * PAGE_SIZE
    );
    
    data_file_.seekp(0, std::ios::end);
    auto current_size = data_file_.tellp();
    
    if (current_size < required_size) {
        // Extend file by writing zero at the end
        data_file_.seekp(required_size - 1, std::ios::beg);
        char zero = 0;
        data_file_.write(&zero, 1);
        
        if (!data_file_) {
            return Err(ErrorCode::WriteError, "Failed to extend data file");
        }
    }
    
    return Ok();
}

} // namespace datyredb::storage
