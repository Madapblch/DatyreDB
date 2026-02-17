// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Write-Ahead Log                                                  ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#pragma once

#include "types.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>
#include <span>

namespace datyredb::storage {

// ==============================================================================
// Log Record
// ==============================================================================

/// WAL log record header (fixed size)
#pragma pack(push, 1)
struct LogRecordHeader {
    // 8-byte types
    Lsn lsn;                     // 8 bytes
    TxnId txn_id;                // 8 bytes
    Lsn prev_lsn;                // 8 bytes
    
    // 4-byte types
    std::uint32_t length;        // 4 bytes
    PageId page_id;              // 4 bytes
    std::uint32_t checksum;      // 4 bytes
    
    // 2-byte types
    std::uint16_t offset;        // 2 bytes
    std::uint16_t data_length;   // 2 bytes
    
    // 1-byte types
    LogRecordType type;          // 1 byte
    std::uint8_t reserved[3];    // 3 bytes (padding to 44 bytes)
    
    static constexpr std::size_t SIZE = 44;
};
#pragma pack(pop)

static_assert(sizeof(LogRecordHeader) == LogRecordHeader::SIZE, "LogRecordHeader size mismatch");

/// Complete log record (header + data)
struct LogRecord {
    LogRecordHeader header;
    std::vector<char> data;  // Before/after images or other data
    
    LogRecord() : header{} {}
    
    LogRecord(LogRecordType type, TxnId txn_id = INVALID_TXN_ID)
        : header{}
    {
        header.type = type;
        header.txn_id = txn_id;
    }
    
    /// Total serialized size
    [[nodiscard]] std::size_t serialized_size() const noexcept {
        return LogRecordHeader::SIZE + data.size();
    }
    
    /// Serialize to buffer
    void serialize(std::vector<char>& buffer) const;
    
    /// Deserialize from buffer
    [[nodiscard]] static Result<LogRecord> deserialize(std::span<const char> buffer);
    
    /// Compute checksum
    [[nodiscard]] std::uint32_t compute_checksum() const noexcept;
    
    /// Verify checksum
    [[nodiscard]] bool verify_checksum() const noexcept;
};

// ==============================================================================
// Write-Ahead Log Manager
// ==============================================================================

class WriteAheadLog {
public:
    // ==========================================================================
    // Construction
    // ==========================================================================
    
    WriteAheadLog(
        std::filesystem::path wal_dir,
        std::size_t segment_size,
        std::shared_ptr<CheckpointMetrics> metrics
    );
    
    ~WriteAheadLog();
    
    // Non-copyable, non-movable
    WriteAheadLog(const WriteAheadLog&) = delete;
    WriteAheadLog& operator=(const WriteAheadLog&) = delete;
    WriteAheadLog(WriteAheadLog&&) = delete;
    WriteAheadLog& operator=(WriteAheadLog&&) = delete;
    
    // ==========================================================================
    // Lifecycle
    // ==========================================================================
    
    /// Open WAL (creates if not exists)
    [[nodiscard]] Status open();
    
    /// Close WAL
    void close();
    
    /// Check if open
    [[nodiscard]] bool is_open() const noexcept { return is_open_; }
    
    // ==========================================================================
    // Writing
    // ==========================================================================
    
    /// Append log record, returns assigned LSN
    [[nodiscard]] Result<Lsn> append(const LogRecord& record);
    
    /// Force WAL to disk up to specified LSN
    [[nodiscard]] Status force(Lsn lsn);
    
    /// Force all buffered data to disk
    [[nodiscard]] Status force_all();
    
    // ==========================================================================
    // Checkpoint Support
    // ==========================================================================
    
    /// Write checkpoint begin record
    [[nodiscard]] Result<Lsn> write_checkpoint_begin();
    
    /// Write checkpoint end record
    [[nodiscard]] Result<Lsn> write_checkpoint_end(Lsn begin_lsn);
    
    /// Truncate WAL segments that contain records strictly older than `lsn`
    [[nodiscard]] Status truncate(Lsn lsn);
    
    // ==========================================================================
    // Transaction Support
    // ==========================================================================
    
    /// Write transaction begin record
    [[nodiscard]] Result<Lsn> write_txn_begin(TxnId txn_id);
    
    /// Write transaction commit record
    [[nodiscard]] Result<Lsn> write_txn_commit(TxnId txn_id);
    
    /// Write transaction abort record
    [[nodiscard]] Result<Lsn> write_txn_abort(TxnId txn_id);
    
    // ==========================================================================
    // Statistics
    // ==========================================================================
    
    /// Current LSN (next to be assigned)
    [[nodiscard]] Lsn current_lsn() const noexcept {
        return next_lsn_.load(std::memory_order_relaxed);
    }
    
    /// Flushed LSN (all records up to this are durable)
    [[nodiscard]] Lsn flushed_lsn() const noexcept {
        return flushed_lsn_.load(std::memory_order_relaxed);
    }
    
    /// Total WAL size in bytes
    [[nodiscard]] std::uint64_t size() const noexcept {
        return total_size_.load(std::memory_order_relaxed);
    }
    
private:
    // ==========================================================================
    // Internal Methods
    // ==========================================================================
    
    /// Rotate to new segment file
    [[nodiscard]] Status rotate_segment();
    
    /// Get path for segment file
    [[nodiscard]] std::filesystem::path segment_path(std::uint64_t segment_id) const;
    
    /// Find existing segments
    [[nodiscard]] std::vector<std::uint64_t> find_segments() const;
    
    // ==========================================================================
    // Data Members
    // ==========================================================================
    
    std::filesystem::path wal_dir_;
    std::size_t segment_size_;
    std::shared_ptr<CheckpointMetrics> metrics_;
    
    std::fstream current_segment_;
    std::uint64_t current_segment_id_{0};
    std::size_t current_segment_pos_{0};
    
    std::atomic<Lsn> next_lsn_{1};
    std::atomic<Lsn> flushed_lsn_{0};
    std::atomic<std::uint64_t> total_size_{0};
    
    std::mutex write_mutex_;
    std::atomic<bool> is_open_{false};
};

} // namespace datyredb::storage
