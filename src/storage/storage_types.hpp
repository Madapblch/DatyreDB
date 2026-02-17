#pragma once

#include <cstdint>
#include <cstddef>
#include <chrono>
#include <atomic>
#include <limits>
#include <string>

namespace datyredb::storage {

// ============================================================================
// Базовые типы
// ============================================================================

/// ID страницы (4 байта = до 16 TB при 4KB страницах)
using PageId = uint32_t;

/// Log Sequence Number (8 байт)
using Lsn = uint64_t;

/// ID транзакции
using TxnId = uint64_t;

/// Размер страницы (4KB — оптимально для SSD/NVMe)
constexpr std::size_t PAGE_SIZE = 4096;

/// Невалидный page ID
constexpr PageId INVALID_PAGE_ID = std::numeric_limits<PageId>::max();

/// Невалидный LSN
constexpr Lsn INVALID_LSN = 0;

// ============================================================================
// Заголовок страницы
// ============================================================================

#pragma pack(push, 1)
struct PageHeader {
    PageId page_id;           // 4 bytes: ID этой страницы
    Lsn page_lsn;             // 8 bytes: LSN последней модификации
    uint16_t free_space;      // 2 bytes: Свободное место
    uint16_t flags;           // 2 bytes: Флаги
    uint32_t checksum;        // 4 bytes: CRC32
    uint32_t reserved;        // 4 bytes: Резерв для выравнивания
    
    static constexpr std::size_t SIZE = 24;
};
#pragma pack(pop)

static_assert(sizeof(PageHeader) == PageHeader::SIZE, "PageHeader size mismatch");

// ============================================================================
// Флаги страницы
// ============================================================================

enum class PageFlags : uint16_t {
    NONE        = 0,
    DIRTY       = 1 << 0,
    PINNED      = 1 << 1,
    LEAF        = 1 << 2,
    INTERNAL    = 1 << 3,
    OVERFLOW    = 1 << 4,
};

inline PageFlags operator|(PageFlags a, PageFlags b) {
    return static_cast<PageFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline PageFlags operator&(PageFlags a, PageFlags b) {
    return static_cast<PageFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline bool has_flag(PageFlags flags, PageFlags flag) {
    return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(flag)) != 0;
}

// ============================================================================
// Типы WAL записей
// ============================================================================

enum class LogRecordType : uint8_t {
    INVALID = 0,
    
    // Data operations
    INSERT,
    UPDATE,
    DELETE,
    
    // Transactions
    TXN_BEGIN,
    TXN_COMMIT,
    TXN_ABORT,
    
    // Checkpoint
    CHECKPOINT_BEGIN,
    CHECKPOINT_END,
    
    // Compensation (for undo)
    CLR,
};

inline const char* log_record_type_name(LogRecordType type) {
    switch (type) {
        case LogRecordType::INVALID: return "INVALID";
        case LogRecordType::INSERT: return "INSERT";
        case LogRecordType::UPDATE: return "UPDATE";
        case LogRecordType::DELETE: return "DELETE";
        case LogRecordType::TXN_BEGIN: return "TXN_BEGIN";
        case LogRecordType::TXN_COMMIT: return "TXN_COMMIT";
        case LogRecordType::TXN_ABORT: return "TXN_ABORT";
        case LogRecordType::CHECKPOINT_BEGIN: return "CHECKPOINT_BEGIN";
        case LogRecordType::CHECKPOINT_END: return "CHECKPOINT_END";
        case LogRecordType::CLR: return "CLR";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Причина checkpoint
// ============================================================================

enum class CheckpointTrigger {
    Timer,              // Периодический по таймеру
    WalSize,            // WAL превысил лимит
    DirtySoftLimit,     // Мягкий лимит dirty pages (фоновый)
    DirtyHardLimit,     // Жёсткий лимит (БЛОКИРУЮЩИЙ!)
    Manual,             // Ручной вызов
    Shutdown,           // При остановке БД
};

inline const char* checkpoint_trigger_name(CheckpointTrigger trigger) {
    switch (trigger) {
        case CheckpointTrigger::Timer: return "timer";
        case CheckpointTrigger::WalSize: return "wal_size";
        case CheckpointTrigger::DirtySoftLimit: return "soft_limit";
        case CheckpointTrigger::DirtyHardLimit: return "HARD_LIMIT";
        case CheckpointTrigger::Manual: return "manual";
        case CheckpointTrigger::Shutdown: return "shutdown";
        default: return "unknown";
    }
}

// ============================================================================
// Метрики Checkpoint
// ============================================================================

struct CheckpointMetrics {
    std::atomic<uint64_t> checkpoint_count{0};
    std::atomic<uint64_t> total_checkpoint_time_ms{0};
    std::atomic<uint64_t> forced_checkpoint_count{0};
    std::atomic<uint64_t> blocking_checkpoint_count{0};
    std::atomic<uint64_t> pages_written_total{0};
    std::atomic<uint64_t> current_wal_size{0};
    std::atomic<std::size_t> dirty_page_count{0};
    
    std::chrono::steady_clock::time_point last_checkpoint_time{
        std::chrono::steady_clock::now()
    };
    
    void record_checkpoint(std::chrono::milliseconds duration, 
                           std::size_t pages, 
                           bool was_forced) {
        checkpoint_count.fetch_add(1, std::memory_order_relaxed);
        total_checkpoint_time_ms.fetch_add(
            static_cast<uint64_t>(duration.count()), 
            std::memory_order_relaxed
        );
        pages_written_total.fetch_add(pages, std::memory_order_relaxed);
        
        if (was_forced) {
            forced_checkpoint_count.fetch_add(1, std::memory_order_relaxed);
        }
        
        last_checkpoint_time = std::chrono::steady_clock::now();
    }
    
    std::chrono::milliseconds avg_checkpoint_time() const {
        uint64_t count = checkpoint_count.load(std::memory_order_relaxed);
        if (count == 0) return std::chrono::milliseconds{0};
        
        uint64_t total = total_checkpoint_time_ms.load(std::memory_order_relaxed);
        return std::chrono::milliseconds{total / count};
    }
    
    std::chrono::seconds time_since_last_checkpoint() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            now - last_checkpoint_time
        );
    }
};

// ============================================================================
// Конфигурация Checkpoint
// ============================================================================

struct CheckpointConfig {
    /// Максимальный интервал между checkpoint'ами
    std::chrono::seconds max_interval{60};
    
    /// Минимальный интервал (защита от checkpoint storm)
    std::chrono::seconds min_interval{5};
    
    /// Максимальный размер WAL до принудительного checkpoint (байты)
    uint64_t max_wal_size = 1ULL * 1024 * 1024 * 1024;  // 1 GB
    
    /// "Мягкий" лимит dirty pages (% от buffer pool)
    float dirty_page_soft_limit_pct = 0.70f;
    
    /// "Жёсткий" лимит dirty pages (% от buffer pool) — БЛОКИРУЮЩИЙ
    float dirty_page_hard_limit_pct = 0.90f;
    
    /// Размер батча для checkpoint (страниц за раз)
    std::size_t checkpoint_batch_size = 256;
    
    /// Throttle delay между батчами (микросекунды)
    std::chrono::microseconds batch_throttle_us{100};
};

// ============================================================================
// Конфигурация всего Storage Layer
// ============================================================================

struct StorageConfig {
    std::string data_path = "./data";
    std::size_t buffer_pool_pages = 10000;  // ~40 MB при 4KB страницах
    std::size_t wal_segment_size = 64 * 1024 * 1024;  // 64 MB
    CheckpointConfig checkpoint;
};

} // namespace datyredb::storage
