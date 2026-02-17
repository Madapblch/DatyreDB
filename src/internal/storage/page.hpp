// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Page Implementation                                              ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#pragma once

#include "types.hpp"

#include <array>
#include <cstring>
#include <memory>
#include <span>

namespace datyredb::storage {

/// Fixed-size page (4KB) with header and payload
class Page {
public:
    // ==========================================================================
    // Construction
    // ==========================================================================
    
    Page() noexcept;
    explicit Page(PageId id) noexcept;
    
    ~Page() = default;
    
    // Non-copyable (pages are managed by buffer pool)
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    
    // Movable
    Page(Page&& other) noexcept;
    Page& operator=(Page&& other) noexcept;
    
    // ==========================================================================
    // Page ID
    // ==========================================================================
    
    [[nodiscard]] PageId id() const noexcept { return id_; }
    void set_id(PageId id) noexcept;
    
    // ==========================================================================
    // Dirty tracking
    // ==========================================================================
    
    [[nodiscard]] bool is_dirty() const noexcept { return dirty_; }
    void mark_dirty() noexcept { dirty_ = true; }
    void mark_clean() noexcept { dirty_ = false; }
    
    // ==========================================================================
    // Pin counting
    // ==========================================================================
    
    [[nodiscard]] int pin_count() const noexcept { return pin_count_; }
    [[nodiscard]] bool is_pinned() const noexcept { return pin_count_ > 0; }
    
    void pin() noexcept { ++pin_count_; }
    void unpin() noexcept { 
        if (pin_count_ > 0) --pin_count_; 
    }
    
    // ==========================================================================
    // LSN (Log Sequence Number)
    // ==========================================================================
    
    [[nodiscard]] Lsn lsn() const noexcept;
    void set_lsn(Lsn lsn) noexcept;
    
    // ==========================================================================
    // Flags
    // ==========================================================================
    
    [[nodiscard]] PageFlags flags() const noexcept;
    void set_flags(PageFlags flags) noexcept;
    void add_flags(PageFlags flags) noexcept;
    void remove_flags(PageFlags flags) noexcept;
    [[nodiscard]] bool has_flags(PageFlags flags) const noexcept;
    
    // ==========================================================================
    // Data access
    // ==========================================================================
    
    /// Raw data pointer (includes header)
    [[nodiscard]] char* data() noexcept { return data_.data(); }
    [[nodiscard]] const char* data() const noexcept { return data_.data(); }
    
    /// Payload pointer (after header)
    [[nodiscard]] char* payload() noexcept { 
        return data_.data() + PageHeader::SIZE; 
    }
    [[nodiscard]] const char* payload() const noexcept { 
        return data_.data() + PageHeader::SIZE; 
    }
    
    /// Payload as span
    [[nodiscard]] std::span<char> payload_span() noexcept {
        return {payload(), payload_size()};
    }
    [[nodiscard]] std::span<const char> payload_span() const noexcept {
        return {payload(), payload_size()};
    }
    
    /// Available payload size
    [[nodiscard]] static constexpr std::size_t payload_size() noexcept {
        return PAGE_SIZE - PageHeader::SIZE;
    }
    
    /// Free space in payload
    [[nodiscard]] std::uint16_t free_space() const noexcept;
    void set_free_space(std::uint16_t space) noexcept;
    
    // ==========================================================================
    // Checksum
    // ==========================================================================
    
    /// Compute CRC32 checksum
    [[nodiscard]] std::uint32_t compute_checksum() const noexcept;
    
    /// Verify checksum matches
    [[nodiscard]] bool verify_checksum() const noexcept;
    
    /// Update stored checksum
    void update_checksum() noexcept;
    
    // ==========================================================================
    // Reset
    // ==========================================================================
    
    /// Reset page to initial state
    void reset() noexcept;
    
    /// Reset and assign new ID
    void reset(PageId new_id) noexcept;
    
private:
    [[nodiscard]] PageHeader* header() noexcept {
        return reinterpret_cast<PageHeader*>(data_.data());
    }
    
    [[nodiscard]] const PageHeader* header() const noexcept {
        return reinterpret_cast<const PageHeader*>(data_.data());
    }
    
    alignas(64) std::array<char, PAGE_SIZE> data_;  // Cache-line aligned
    PageId id_{INVALID_PAGE_ID};
    bool dirty_{false};
    int pin_count_{0};
};

static_assert(sizeof(Page) <= PAGE_SIZE + 64, "Page object too large");

} // namespace datyredb::storage
