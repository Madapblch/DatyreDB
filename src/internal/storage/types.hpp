// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Storage Layer Types                                              ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#pragma once

#include <cstdint>
#include <cstddef>
#include <chrono>
#include <atomic>
#include <limits>
#include <string>
#include <vector>
#include <variant>
#include <stdexcept>
#include <utility>
#include <type_traits>

namespace datyredb::storage {

// ==============================================================================
// Fundamental Types
// ==============================================================================

/// Page identifier (4 bytes = supports up to 16TB at 4KB pages)
using PageId = std::uint32_t;

/// Log Sequence Number (8 bytes = virtually unlimited)
using Lsn = std::uint64_t;

/// Transaction identifier
using TxnId = std::uint64_t;

/// Frame identifier in buffer pool
using FrameId = std::uint32_t;

/// Page size (4KB - optimal for modern SSDs/NVMe)
inline constexpr std::size_t PAGE_SIZE = 4096;

/// Invalid page ID sentinel
inline constexpr PageId INVALID_PAGE_ID = std::numeric_limits<PageId>::max();

/// Invalid LSN sentinel
inline constexpr Lsn INVALID_LSN = 0;

/// Invalid transaction ID sentinel
inline constexpr TxnId INVALID_TXN_ID = 0;

/// Invalid frame ID sentinel
inline constexpr FrameId INVALID_FRAME_ID = std::numeric_limits<FrameId>::max();

// ==============================================================================
// Error Handling
// ==============================================================================

/// Storage error codes
enum class ErrorCode : std::uint32_t {
    Success = 0,
    IoError = 100,
    FileNotFound = 101,
    PermissionDenied = 102,
    DiskFull = 103,
    ReadError = 104,
    WriteError = 105,
    SyncError = 106,
    PageNotFound = 200,
    PageCorrupted = 201,
    ChecksumMismatch = 202,
    InvalidPageId = 203,
    PagePinned = 204,
    BufferPoolFull = 300,
    NoAvailableFrames = 301,
    FrameNotFound = 302,
    WalCorrupted = 400,
    WalFull = 401,
    InvalidLsn = 402,
    TransactionAborted = 500,
    DeadlockDetected = 501,
    CheckpointFailed = 600,
    RecoveryFailed = 601,
    InvalidArgument = 900,
    NotImplemented = 901,
    InternalError = 999,
};

/// Convert error code to string
[[nodiscard]] constexpr const char* error_code_to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::IoError: return "I/O Error";
        case ErrorCode::FileNotFound: return "File not found";
        case ErrorCode::PermissionDenied: return "Permission denied";
        case ErrorCode::DiskFull: return "Disk full";
        case ErrorCode::ReadError: return "Read error";
        case ErrorCode::WriteError: return "Write error";
        case ErrorCode::SyncError: return "Sync error";
        case ErrorCode::PageNotFound: return "Page not found";
        case ErrorCode::PageCorrupted: return "Page corrupted";
        case ErrorCode::ChecksumMismatch: return "Checksum mismatch";
        case ErrorCode::InvalidPageId: return "Invalid page ID";
        case ErrorCode::PagePinned: return "Page is pinned";
        case ErrorCode::BufferPoolFull: return "Buffer pool full";
        case ErrorCode::NoAvailableFrames: return "No available frames";
        case ErrorCode::FrameNotFound: return "Frame not found";
        case ErrorCode::WalCorrupted: return "WAL corrupted";
        case ErrorCode::WalFull: return "WAL full";
        case ErrorCode::InvalidLsn: return "Invalid LSN";
        case ErrorCode::TransactionAborted: return "Transaction aborted";
        case ErrorCode::DeadlockDetected: return "Deadlock detected";
        case ErrorCode::CheckpointFailed: return "Checkpoint failed";
        case ErrorCode::RecoveryFailed: return "Recovery failed";
        case ErrorCode::InvalidArgument: return "Invalid argument";
        case ErrorCode::NotImplemented: return "Not implemented";
        case ErrorCode::InternalError: return "Internal error";
        default: return "Unknown error";
    }
}

/// Storage error with context
class Error {
public:
    Error() noexcept : code_(ErrorCode::Success) {}
    
    explicit Error(ErrorCode code) noexcept : code_(code) {}
    
    Error(ErrorCode code, std::string message) noexcept 
        : code_(code), message_(std::move(message)) {}
    
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] bool ok() const noexcept { return code_ == ErrorCode::Success; }
    [[nodiscard]] explicit operator bool() const noexcept { return !ok(); }
    
    [[nodiscard]] std::string to_string() const {
        if (message_.empty()) {
            return std::string(error_code_to_string(code_));
        }
        return std::string(error_code_to_string(code_)) + ": " + message_;
    }
    
private:
    ErrorCode code_;
    std::string message_;
};

// ==============================================================================
// Result<T> - C++20 compatible Result type
// ==============================================================================

/// Tag type for constructing error result
struct ErrorTag {};
inline constexpr ErrorTag error_tag{};

/// Result type for operations that can fail
template<typename T>
class Result {
public:
    using value_type = T;
    using error_type = Error;
    
    // Success constructors
    Result(const T& value) : storage_(value), has_value_(true) {}
    Result(T&& value) : storage_(std::move(value)), has_value_(true) {}
    
    // Error constructors
    Result(ErrorTag, const Error& err) : storage_(err), has_value_(false) {}
    Result(ErrorTag, Error&& err) : storage_(std::move(err)), has_value_(false) {}
    Result(ErrorTag, ErrorCode code) : storage_(Error(code)), has_value_(false) {}
    Result(ErrorTag, ErrorCode code, std::string msg) 
        : storage_(Error(code, std::move(msg))), has_value_(false) {}
    
    // Copy/Move
    Result(const Result&) = default;
    Result(Result&&) noexcept = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) noexcept = default;
    
    // Observers
    [[nodiscard]] bool has_value() const noexcept { return has_value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value_; }
    
    [[nodiscard]] T& value() & {
        if (!has_value_) throw std::runtime_error("Result has no value");
        return std::get<T>(storage_);
    }
    
    [[nodiscard]] const T& value() const& {
        if (!has_value_) throw std::runtime_error("Result has no value");
        return std::get<T>(storage_);
    }
    
    [[nodiscard]] T&& value() && {
        if (!has_value_) throw std::runtime_error("Result has no value");
        return std::get<T>(std::move(storage_));
    }
    
    [[nodiscard]] const Error& error() const& {
        if (has_value_) throw std::runtime_error("Result has value, not error");
        return std::get<Error>(storage_);
    }
    
    // Accessors
    [[nodiscard]] T* operator->() { return &value(); }
    [[nodiscard]] const T* operator->() const { return &value(); }
    [[nodiscard]] T& operator*() & { return value(); }
    [[nodiscard]] const T& operator*() const& { return value(); }
    [[nodiscard]] T&& operator*() && { return std::move(value()); }
    
    // Value or default
    template<typename U>
    [[nodiscard]] T value_or(U&& default_value) const& {
        return has_value_ ? value() : static_cast<T>(std::forward<U>(default_value));
    }
    
    template<typename U>
    [[nodiscard]] T value_or(U&& default_value) && {
        return has_value_ ? std::move(value()) : static_cast<T>(std::forward<U>(default_value));
    }
    
private:
    std::variant<T, Error> storage_;
    bool has_value_;
};

// ==============================================================================
// Result<void> specialization (Status)
// ==============================================================================

template<>
class Result<void> {
public:
    using value_type = void;
    using error_type = Error;
    
    // Success constructor
    Result() : error_(), has_value_(true) {}
    
    // Error constructors
    Result(ErrorTag, const Error& err) : error_(err), has_value_(false) {}
    Result(ErrorTag, Error&& err) : error_(std::move(err)), has_value_(false) {}
    Result(ErrorTag, ErrorCode code) : error_(code), has_value_(false) {}
    Result(ErrorTag, ErrorCode code, std::string msg) 
        : error_(code, std::move(msg)), has_value_(false) {}
    
    // Copy/Move
    Result(const Result&) = default;
    Result(Result&&) noexcept = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) noexcept = default;
    
    // Observers
    [[nodiscard]] bool has_value() const noexcept { return has_value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value_; }
    
    void value() const {
        if (!has_value_) throw std::runtime_error("Result has no value");
    }
    
    [[nodiscard]] const Error& error() const& {
        if (has_value_) throw std::runtime_error("Result has value, not error");
        return error_;
    }
    
private:
    Error error_;
    bool has_value_;
};

/// Status is an alias for Result<void>
using Status = Result<void>;

// ==============================================================================
// Factory functions
// ==============================================================================

/// Create success result with value
template<typename T>
[[nodiscard]] Result<std::decay_t<T>> Ok(T&& value) {
    return Result<std::decay_t<T>>(std::forward<T>(value));
}

/// Create success status (void result)
[[nodiscard]] inline Status Ok() {
    return Status();
}

/// Create error result
template<typename T>
[[nodiscard]] Result<T> Err(ErrorCode code) {
    return Result<T>(error_tag, code);
}

template<typename T>
[[nodiscard]] Result<T> Err(ErrorCode code, std::string message) {
    return Result<T>(error_tag, code, std::move(message));
}

template<typename T>
[[nodiscard]] Result<T> Err(const Error& error) {
    return Result<T>(error_tag, error);
}

/// Create error status
[[nodiscard]] inline Status Err(ErrorCode code) {
    return Status(error_tag, code);
}

[[nodiscard]] inline Status Err(ErrorCode code, std::string message) {
    return Status(error_tag, code, std::move(message));
}

[[nodiscard]] inline Status Err(const Error& error) {
    return Status(error_tag, error);
}

// ==============================================================================
// Page Header
// ==============================================================================

#pragma pack(push, 1)
struct PageHeader {
    PageId page_id;              // 4 bytes
    Lsn page_lsn;                // 8 bytes
    std::uint16_t free_space;    // 2 bytes
    std::uint16_t flags;         // 2 bytes
    std::uint32_t checksum;      // 4 bytes
    std::uint32_t reserved;      // 4 bytes (alignment + future use)
    
    static constexpr std::size_t SIZE = 24;
};
#pragma pack(pop)

static_assert(sizeof(PageHeader) == PageHeader::SIZE, "PageHeader size mismatch");
static_assert(PageHeader::SIZE <= PAGE_SIZE / 16, "PageHeader too large");

// ==============================================================================
// Page Flags
// ==============================================================================

enum class PageFlags : std::uint16_t {
    None        = 0,
    Dirty       = 1 << 0,
    Pinned      = 1 << 1,
    New         = 1 << 2,
    Leaf        = 1 << 3,
    Internal    = 1 << 4,
    Overflow    = 1 << 5,
    Root        = 1 << 6,
    Deleted     = 1 << 7,
};

[[nodiscard]] constexpr PageFlags operator|(PageFlags lhs, PageFlags rhs) noexcept {
    using T = std::underlying_type_t<PageFlags>;
    return static_cast<PageFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

[[nodiscard]] constexpr PageFlags operator&(PageFlags lhs, PageFlags rhs) noexcept {
    using T = std::underlying_type_t<PageFlags>;
    return static_cast<PageFlags>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

[[nodiscard]] constexpr PageFlags operator~(PageFlags flags) noexcept {
    using T = std::underlying_type_t<PageFlags>;
    return static_cast<PageFlags>(~static_cast<T>(flags));
}

constexpr PageFlags& operator|=(PageFlags& lhs, PageFlags rhs) noexcept {
    return lhs = lhs | rhs;
}

constexpr PageFlags& operator&=(PageFlags& lhs, PageFlags rhs) noexcept {
    return lhs = lhs & rhs;
}

[[nodiscard]] constexpr bool has_flag(PageFlags flags, PageFlags flag) noexcept {
    using T = std::underlying_type_t<PageFlags>;
    return (static_cast<T>(flags) & static_cast<T>(flag)) != 0;
}

// ==============================================================================
// WAL Record Types
// ==============================================================================

enum class LogRecordType : std::uint8_t {
    Invalid = 0,
    
    // Data operations
    Insert = 1,
    Update = 2,
    Delete = 3,
    
    // Page operations
    PageAlloc = 10,
    PageFree = 11,
    PageInit = 12,
    
    // Transaction control
    TxnBegin = 20,
    TxnCommit = 21,
    TxnAbort = 22,
    TxnPrepare = 23,  // For 2PC
    
    // Checkpoint
    CheckpointBegin = 30,
    CheckpointEnd = 31,
    
    // Compensation (for ARIES undo)
    Clr = 40,
    
    // System
    Noop = 255,
};

[[nodiscard]] constexpr const char* log_record_type_to_string(LogRecordType type) noexcept {
    switch (type) {
        case LogRecordType::Invalid: return "INVALID";
        case LogRecordType::Insert: return "INSERT";
        case LogRecordType::Update: return "UPDATE";
        case LogRecordType::Delete: return "DELETE";
        case LogRecordType::PageAlloc: return "PAGE_ALLOC";
        case LogRecordType::PageFree: return "PAGE_FREE";
        case LogRecordType::PageInit: return "PAGE_INIT";
        case LogRecordType::TxnBegin: return "TXN_BEGIN";
        case LogRecordType::TxnCommit: return "TXN_COMMIT";
        case LogRecordType::TxnAbort: return "TXN_ABORT";
        case LogRecordType::TxnPrepare: return "TXN_PREPARE";
        case LogRecordType::CheckpointBegin: return "CHECKPOINT_BEGIN";
        case LogRecordType::CheckpointEnd: return "CHECKPOINT_END";
        case LogRecordType::Clr: return "CLR";
        case LogRecordType::Noop: return "NOOP";
        default: return "UNKNOWN";
    }
}

// ==============================================================================
// Checkpoint Trigger
// ==============================================================================

enum class CheckpointTrigger : std::uint8_t {
    Timer,           // Periodic timer
    WalSize,         // WAL size threshold
    DirtySoft,       // Soft dirty page limit (background)
    DirtyHard,       // Hard dirty page limit (blocking)
    Manual,          // User requested
    Shutdown,        // Graceful shutdown
};

[[nodiscard]] constexpr const char* checkpoint_trigger_to_string(CheckpointTrigger trigger) noexcept {
    switch (trigger) {
        case CheckpointTrigger::Timer: return "timer";
        case CheckpointTrigger::WalSize: return "wal_size";
        case CheckpointTrigger::DirtySoft: return "dirty_soft";
        case CheckpointTrigger::DirtyHard: return "DIRTY_HARD";
        case CheckpointTrigger::Manual: return "manual";
        case CheckpointTrigger::Shutdown: return "shutdown";
        default: return "unknown";
    }
}

// ==============================================================================
// Metrics
// ==============================================================================

struct CheckpointMetrics {
    std::atomic<std::uint64_t> checkpoint_count{0};
    std::atomic<std::uint64_t> total_duration_us{0};
    std::atomic<std::uint64_t> pages_written{0};
    std::atomic<std::uint64_t> forced_count{0};
    std::atomic<std::uint64_t> blocking_count{0};
    
    std::atomic<std::uint64_t> wal_size{0};
    std::atomic<std::size_t> dirty_pages{0};
    
    std::chrono::steady_clock::time_point last_checkpoint{std::chrono::steady_clock::now()};
    
    void record(std::chrono::microseconds duration, std::size_t pages, bool forced, bool blocking) {
        checkpoint_count.fetch_add(1, std::memory_order_relaxed);
        total_duration_us.fetch_add(static_cast<std::uint64_t>(duration.count()), std::memory_order_relaxed);
        pages_written.fetch_add(pages, std::memory_order_relaxed);
        
        if (forced) {
            forced_count.fetch_add(1, std::memory_order_relaxed);
        }
        if (blocking) {
            blocking_count.fetch_add(1, std::memory_order_relaxed);
        }
        
        last_checkpoint = std::chrono::steady_clock::now();
    }
    
    [[nodiscard]] std::chrono::microseconds average_duration() const noexcept {
        auto count = checkpoint_count.load(std::memory_order_relaxed);
        if (count == 0) return std::chrono::microseconds{0};
        return std::chrono::microseconds{total_duration_us.load(std::memory_order_relaxed) / count};
    }
    
    [[nodiscard]] std::chrono::seconds since_last() const noexcept {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - last_checkpoint
        );
    }
};

// ==============================================================================
// Configuration
// ==============================================================================

struct CheckpointConfig {
    std::chrono::seconds max_interval{60};
    std::chrono::seconds min_interval{5};
    std::uint64_t max_wal_size{1ULL << 30};        // 1 GB
    float dirty_soft_limit{0.70f};                   // 70%
    float dirty_hard_limit{0.90f};                   // 90%
    std::size_t batch_size{256};
    std::chrono::microseconds batch_throttle{100};
};

struct StorageConfig {
    std::string data_directory{"./data"};
    std::size_t buffer_pool_size{10000};            // Pages
    std::size_t wal_segment_size{64 << 20};         // 64 MB
    bool sync_on_commit{true};
    bool direct_io{false};
    CheckpointConfig checkpoint;
};

} // namespace datyredb::storage
