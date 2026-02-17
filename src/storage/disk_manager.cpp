#include "storage/disk_manager.hpp"
#include "utils/logger.hpp"

#include <cstring>

namespace datyredb::storage {

DiskManager::DiskManager(const std::filesystem::path& db_path)
    : db_path_(db_path)
    , data_file_path_(db_path / "data.db")
{
}

DiskManager::~DiskManager() {
    shutdown();
}

bool DiskManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Создаём директорию если нужно
    std::error_code ec;
    std::filesystem::create_directories(db_path_, ec);
    if (ec) {
        Logger::error("DiskManager: failed to create directory {}: {}",
                      db_path_.string(), ec.message());
        return false;
    }
    
    // Открываем файл данных
    data_file_.open(data_file_path_, 
                    std::ios::in | std::ios::out | std::ios::binary);
    
    if (!data_file_.is_open()) {
        // Файл не существует — создаём новый
        data_file_.open(data_file_path_, 
                        std::ios::out | std::ios::binary);
        if (!data_file_.is_open()) {
            Logger::error("DiskManager: failed to create data file: {}",
                          data_file_path_.string());
            return false;
        }
        data_file_.close();
        
        // Переоткрываем для чтения/записи
        data_file_.open(data_file_path_,
                        std::ios::in | std::ios::out | std::ios::binary);
    }
    
    if (!data_file_.is_open()) {
        Logger::error("DiskManager: failed to open data file: {}",
                      data_file_path_.string());
        return false;
    }
    
    // Определяем количество существующих страниц
    data_file_.seekg(0, std::ios::end);
    auto file_size = static_cast<uint64_t>(data_file_.tellg());
    next_page_id_.store(static_cast<PageId>(file_size / PAGE_SIZE));
    
    initialized_ = true;
    
    Logger::info("DiskManager initialized: path={}, pages={}", 
                 data_file_path_.string(), 
                 next_page_id_.load());
    
    return true;
}

void DiskManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    std::lock_guard lock(io_mutex_);
    
    if (data_file_.is_open()) {
        data_file_.flush();
        data_file_.close();
    }
    
    initialized_ = false;
    Logger::info("DiskManager shutdown");
}

bool DiskManager::read_page(PageId page_id, Page& page) {
    std::lock_guard lock(io_mutex_);
    
    if (page_id >= next_page_id_.load()) {
        Logger::error("DiskManager: read invalid page_id={} (max={})", 
                      page_id, next_page_id_.load());
        return false;
    }
    
    auto offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;
    data_file_.seekg(offset);
    
    if (!data_file_) {
        Logger::error("DiskManager: seekg failed for page {}", page_id);
        return false;
    }
    
    if (!data_file_.read(page.data(), PAGE_SIZE)) {
        Logger::error("DiskManager: read failed for page {}", page_id);
        return false;
    }
    
    page.set_page_id(page_id);
    page.mark_clean();
    
    // Проверка checksum
    if (!page.verify_checksum()) {
        Logger::error("DiskManager: checksum mismatch for page {}", page_id);
        return false;
    }
    
    return true;
}

bool DiskManager::write_page(PageId page_id, const Page& page) {
    std::lock_guard lock(io_mutex_);
    
    // Обновляем checksum перед записью
    Page& mutable_page = const_cast<Page&>(page);
    mutable_page.update_checksum();
    
    auto offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;
    data_file_.seekp(offset);
    
    if (!data_file_) {
        Logger::error("DiskManager: seekp failed for page {}", page_id);
        return false;
    }
    
    if (!data_file_.write(page.data(), PAGE_SIZE)) {
        Logger::error("DiskManager: write failed for page {}", page_id);
        return false;
    }
    
    return true;
}

PageId DiskManager::allocate_page() {
    PageId new_id = next_page_id_.fetch_add(1);
    
    // Расширяем файл
    {
        std::lock_guard lock(io_mutex_);
        auto offset = static_cast<std::streamoff>(new_id + 1) * PAGE_SIZE - 1;
        data_file_.seekp(offset);
        char zero = 0;
        data_file_.write(&zero, 1);
        data_file_.flush();
    }
    
    Logger::debug("DiskManager: allocated page {}", new_id);
    return new_id;
}

void DiskManager::deallocate_page(PageId page_id) {
    // TODO: implement free list
    Logger::debug("DiskManager: deallocated page {}", page_id);
}

void DiskManager::sync() {
    std::lock_guard lock(io_mutex_);
    if (data_file_.is_open()) {
        data_file_.flush();
    }
}

uint64_t DiskManager::data_file_size() const {
    return static_cast<uint64_t>(next_page_id_.load()) * PAGE_SIZE;
}

} // namespace datyredb::storage
