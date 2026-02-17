#pragma once

#include "storage/storage_types.hpp"

#include <array>
#include <cstring>
#include <memory>

namespace datyredb::storage {

/// Страница фиксированного размера (4KB)
class Page {
public:
    Page();
    explicit Page(PageId id);
    
    // ========================================================================
    // Accessors
    // ========================================================================
    
    PageId page_id() const { return page_id_; }
    void set_page_id(PageId id);
    
    bool is_dirty() const { return is_dirty_; }
    void mark_dirty() { is_dirty_ = true; }
    void mark_clean() { is_dirty_ = false; }
    
    int pin_count() const { return pin_count_; }
    void pin() { ++pin_count_; }
    void unpin() { if (pin_count_ > 0) --pin_count_; }
    bool is_pinned() const { return pin_count_ > 0; }
    
    Lsn get_lsn() const;
    void set_lsn(Lsn lsn);
    
    // ========================================================================
    // Data access
    // ========================================================================
    
    char* data() { return data_.data(); }
    const char* data() const { return data_.data(); }
    
    /// Данные после заголовка
    char* payload() { return data_.data() + PageHeader::SIZE; }
    const char* payload() const { return data_.data() + PageHeader::SIZE; }
    
    static constexpr std::size_t payload_size() { 
        return PAGE_SIZE - PageHeader::SIZE; 
    }
    
    // ========================================================================
    // Checksum
    // ========================================================================
    
    uint32_t compute_checksum() const;
    bool verify_checksum() const;
    void update_checksum();
    
    // ========================================================================
    // Reset
    // ========================================================================
    
    void reset();
    
private:
    PageHeader* header();
    const PageHeader* header() const;
    
    std::array<char, PAGE_SIZE> data_;
    PageId page_id_;
    bool is_dirty_;
    int pin_count_;
};

using PagePtr = std::shared_ptr<Page>;

} // namespace datyredb::storage
