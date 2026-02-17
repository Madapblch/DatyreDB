// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Page Implementation                                              ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "page.hpp"

#include <algorithm>
#include <cstring>
#include <array>

namespace datyredb::storage {

namespace {

// Compile-time CRC32 table generation
// Polynomial: 0xEDB88320 (IEEE 802.3)
constexpr auto generate_crc32_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        table[i] = crc;
    }
    return table;
}

constexpr auto CRC32_TABLE = generate_crc32_table();

// Helper to compute CRC32 used internally in this translation unit
[[maybe_unused]] [[nodiscard]] static std::uint32_t compute_crc32(const void* data, std::size_t length) noexcept {
    auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint32_t crc = 0xFFFFFFFF;
    
    for (std::size_t i = 0; i < length; ++i) {
        crc = CRC32_TABLE[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

} // anonymous namespace

// ==============================================================================
// Construction
// ==============================================================================

Page::Page() noexcept {
    reset();
}

Page::Page(PageId id) noexcept {
    reset(id);
}

Page::Page(Page&& other) noexcept
    : data_(std::move(other.data_))
    , id_(other.id_)
    , dirty_(other.dirty_)
    , pin_count_(other.pin_count_)
{
    other.id_ = INVALID_PAGE_ID;
    other.dirty_ = false;
    other.pin_count_ = 0;
}

Page& Page::operator=(Page&& other) noexcept {
    if (this != &other) {
        data_ = std::move(other.data_);
        id_ = other.id_;
        dirty_ = other.dirty_;
        pin_count_ = other.pin_count_;
        
        other.id_ = INVALID_PAGE_ID;
        other.dirty_ = false;
        other.pin_count_ = 0;
    }
    return *this;
}

// ==============================================================================
// Page ID
// ==============================================================================

void Page::set_id(PageId id) noexcept {
    id_ = id;
    header()->page_id = id;
}

// ==============================================================================
// LSN
// ==============================================================================

Lsn Page::lsn() const noexcept {
    return header()->page_lsn;
}

void Page::set_lsn(Lsn lsn) noexcept {
    header()->page_lsn = lsn;
}

// ==============================================================================
// Flags
// ==============================================================================

PageFlags Page::flags() const noexcept {
    return static_cast<PageFlags>(header()->flags);
}

void Page::set_flags(PageFlags flags) noexcept {
    header()->flags = static_cast<std::uint16_t>(flags);
}

void Page::add_flags(PageFlags flags) noexcept {
    // Используем перегруженный operator| из types.hpp
    header()->flags = static_cast<std::uint16_t>(this->flags() | flags);
}

void Page::remove_flags(PageFlags flags) noexcept {
    // Используем перегруженный operator~ и operator& из types.hpp
    // Приводим к uint16_t явно, чтобы избежать warnings
    auto new_flags = this->flags() & ~flags;
    header()->flags = static_cast<std::uint16_t>(new_flags);
}

bool Page::has_flags(PageFlags flags) const noexcept {
    return has_flag(this->flags(), flags);
}

// ==============================================================================
// Free space
// ==============================================================================

std::uint16_t Page::free_space() const noexcept {
    return header()->free_space;
}

void Page::set_free_space(std::uint16_t space) noexcept {
    header()->free_space = space;
}

// ==============================================================================
// Checksum
// ==============================================================================

std::uint32_t Page::compute_checksum() const noexcept {
    // Compute CRC32 over entire page except checksum field
    constexpr std::size_t checksum_offset = offsetof(PageHeader, checksum);
    
    std::uint32_t crc = 0xFFFFFFFF;
    auto* bytes = reinterpret_cast<const std::uint8_t*>(data_.data());
    
    for (std::size_t i = 0; i < PAGE_SIZE; ++i) {
        // Skip the checksum field bytes
        if (i >= checksum_offset && i < checksum_offset + sizeof(std::uint32_t)) {
            continue;
        }
        crc = CRC32_TABLE[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

bool Page::verify_checksum() const noexcept {
    return compute_checksum() == header()->checksum;
}

void Page::update_checksum() noexcept {
    header()->checksum = compute_checksum();
}

// ==============================================================================
// Reset
// ==============================================================================

void Page::reset() noexcept {
    std::memset(data_.data(), 0, PAGE_SIZE);
    id_ = INVALID_PAGE_ID;
    dirty_ = false;
    pin_count_ = 0;
    
    header()->free_space = static_cast<std::uint16_t>(payload_size());
}

void Page::reset(PageId new_id) noexcept {
    reset();
    set_id(new_id);
}

} // namespace datyredb::storage
