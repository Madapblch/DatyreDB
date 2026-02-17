// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Write-Ahead Log Implementation                                   ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "wal.hpp"

#include <fmt/core.h>
#include <algorithm>
#include <cstring>
#include <regex>
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

[[nodiscard]] std::uint32_t compute_crc32(const void* data, std::size_t length) noexcept {
    auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint32_t crc = 0xFFFFFFFF;
    
    for (std::size_t i = 0; i < length; ++i) {
        crc = CRC32_TABLE[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

} // anonymous namespace

// ==============================================================================
// LogRecord
// ==============================================================================

void LogRecord::serialize(std::vector<char>& buffer) const {
    buffer.resize(serialized_size());
    char* ptr = buffer.data();
    
    // Copy header (update length and checksum)
    LogRecordHeader hdr = header;
    hdr.length = static_cast<std::uint32_t>(serialized_size());
    hdr.data_length = static_cast<std::uint16_t>(data.size());
    hdr.checksum = 0;  // Will be computed below
    
    std::memcpy(ptr, &hdr, LogRecordHeader::SIZE);
    ptr += LogRecordHeader::SIZE;
    
    // Copy data
    if (!data.empty()) {
        std::memcpy(ptr, data.data(), data.size());
    }
    
    // Compute and store checksum
    std::uint32_t checksum = compute_crc32(buffer.data(), buffer.size());
    std::memcpy(buffer.data() + offsetof(LogRecordHeader, checksum), &checksum, sizeof(checksum));
}

Result<LogRecord> LogRecord::deserialize(std::span<const char> buffer) {
    if (buffer.size() < LogRecordHeader::SIZE) {
        return Err<LogRecord>(ErrorCode::WalCorrupted, "Buffer too small for header");
    }
    
    LogRecord record;
    std::memcpy(&record.header, buffer.data(), LogRecordHeader::SIZE);
    
    if (record.header.length > buffer.size()) {
        return Err<LogRecord>(ErrorCode::WalCorrupted, "Incomplete record");
    }
    
    // Copy data
    if (record.header.data_length > 0) {
        record.data.resize(record.header.data_length);
        std::memcpy(record.data.data(), 
            buffer.data() + LogRecordHeader::SIZE, 
            record.header.data_length);
    }
    
    // Verify checksum
    if (!record.verify_checksum()) {
        return Err<LogRecord>(ErrorCode::WalCorrupted, "Checksum mismatch");
    }
    
    return Ok(std::move(record));
}

std::uint32_t LogRecord::compute_checksum() const noexcept {
    std::vector<char> buffer;
    buffer.resize(serialized_size());
    char* ptr = buffer.data();
    
    LogRecordHeader hdr = header;
    hdr.checksum = 0;
    std::memcpy(ptr, &hdr, LogRecordHeader::SIZE);
    
    if (!data.empty()) {
        std::memcpy(ptr + LogRecordHeader::SIZE, data.data(), data.size());
    }
    
    return compute_crc32(buffer.data(), buffer.size());
}

bool LogRecord::verify_checksum() const noexcept {
    return compute_checksum() == header.checksum;
}

// ==============================================================================
// WriteAheadLog
// ==============================================================================

WriteAheadLog::WriteAheadLog(
    std::filesystem::path wal_dir,
    std::size_t segment_size,
    std::shared_ptr<CheckpointMetrics> metrics
)
    : wal_dir_(std::move(wal_dir))
    , segment_size_(segment_size)
    , metrics_(std::move(metrics))
{
}

WriteAheadLog::~WriteAheadLog() {
    close();
}

Status WriteAheadLog::open() {
    if (is_open_) {
        return Ok();
    }
    
    std::lock_guard lock(write_mutex_);
    
    // Create directory
    std::error_code ec;
    std::filesystem::create_directories(wal_dir_, ec);
    if (ec) {
        return Err(ErrorCode::IoError,
            fmt::format("Failed to create WAL directory '{}': {}",
                wal_dir_.string(), ec.message()));
    }
    
    // Find existing segments
    auto segments = find_segments();
    
    if (segments.empty()) {
        current_segment_id_ = 0;
    } else {
        current_segment_id_ = segments.back();
    }
    
    // Open current segment
    auto path = segment_path(current_segment_id_);
    current_segment_.open(path,
        std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
    
    if (!current_segment_.is_open()) {
        // Create new segment
        current_segment_.open(path, std::ios::out | std::ios::binary);
        if (!current_segment_.is_open()) {
            return Err(ErrorCode::IoError,
                fmt::format("Failed to create WAL segment '{}'", path.string()));
        }
        current_segment_.close();
        current_segment_.open(path,
            std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
    }
    
    // Get current position
    current_segment_.seekp(0, std::ios::end);
    current_segment_pos_ = static_cast<std::size_t>(current_segment_.tellp());
    
    // Calculate total size
    std::uint64_t total = 0;
    for (auto seg_id : segments) {
        auto seg_path = segment_path(seg_id);
        if (std::filesystem::exists(seg_path)) {
            total += std::filesystem::file_size(seg_path);
        }
    }
    total_size_.store(total, std::memory_order_relaxed);
    
    if (metrics_) {
        metrics_->wal_size.store(total, std::memory_order_relaxed);
    }
    
    is_open_ = true;
    return Ok();
}

void WriteAheadLog::close() {
    if (!is_open_) {
        return;
    }
    
    std::lock_guard lock(write_mutex_);
    
    if (current_segment_.is_open()) {
        current_segment_.flush();
        current_segment_.close();
    }
    
    is_open_ = false;
}

Result<Lsn> WriteAheadLog::append(const LogRecord& record) {
    if (!is_open_) {
        return Err<Lsn>(ErrorCode::IoError, "WAL not open");
    }
    
    std::lock_guard lock(write_mutex_);
    
    // Assign LSN
    LogRecord rec = record;
    rec.header.lsn = next_lsn_.fetch_add(1, std::memory_order_relaxed);
    
    // Serialize
    std::vector<char> buffer;
    rec.serialize(buffer);
    
    // Check if we need to rotate
    if (current_segment_pos_ + buffer.size() > segment_size_) {
        if (auto status = rotate_segment(); !status) {
            return Err<Lsn>(status.error().code(), status.error().message());
        }
    }
    
    // Write
    current_segment_.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (!current_segment_) {
        return Err<Lsn>(ErrorCode::WriteError, "Failed to write to WAL");
    }
    
    current_segment_pos_ += buffer.size();
    
    auto new_size = total_size_.fetch_add(buffer.size(), std::memory_order_relaxed) + buffer.size();
    if (metrics_) {
        metrics_->wal_size.store(new_size, std::memory_order_relaxed);
    }
    
    return Ok(rec.header.lsn);
}

Status WriteAheadLog::force(Lsn lsn) {
    if (!is_open_) {
        return Err(ErrorCode::IoError, "WAL not open");
    }
    
    std::lock_guard lock(write_mutex_);
    
    current_segment_.flush();
    if (!current_segment_) {
        return Err(ErrorCode::SyncError, "Failed to flush WAL");
    }
    
    flushed_lsn_.store(lsn, std::memory_order_release);
    return Ok();
}

Status WriteAheadLog::force_all() {
    return force(next_lsn_.load(std::memory_order_relaxed) - 1);
}

Result<Lsn> WriteAheadLog::write_checkpoint_begin() {
    LogRecord record(LogRecordType::CheckpointBegin);
    return append(record);
}

Result<Lsn> WriteAheadLog::write_checkpoint_end(Lsn begin_lsn) {
    LogRecord record(LogRecordType::CheckpointEnd);
    record.header.prev_lsn = begin_lsn;
    
    auto result = append(record);
    if (result) {
        if (auto status = force(*result); !status) {
            return Err<Lsn>(status.error().code(), status.error().message());
        }
    }
    
    return result;
}

Status WriteAheadLog::truncate(Lsn lsn) {
    if (!is_open_) {
        return Err(ErrorCode::IoError, "WAL not open");
    }
    
    // Explicitly use lsn to avoid warning and indicate intent
    (void)lsn; 
    
    std::lock_guard lock(write_mutex_);
    
    // Find segments to remove
    auto segments = find_segments();
    
    std::uint64_t freed = 0;
    
    // Safety check: always keep at least 2 most recent segments
    if (segments.size() <= 2) {
        return Ok();
    }
    
    // Remove all but last 2 segments
    for (size_t i = 0; i < segments.size() - 2; ++i) {
        auto seg_id = segments[i];
        
        // Skip current segment just in case
        if (seg_id == current_segment_id_) continue;
        
        auto path = segment_path(seg_id);
        if (std::filesystem::exists(path)) {
            freed += std::filesystem::file_size(path);
            std::filesystem::remove(path);
        }
    }
    
    if (freed > 0) {
        auto new_size = total_size_.fetch_sub(freed, std::memory_order_relaxed) - freed;
        if (metrics_) {
            metrics_->wal_size.store(new_size, std::memory_order_relaxed);
        }
    }
    
    return Ok();
}

Result<Lsn> WriteAheadLog::write_txn_begin(TxnId txn_id) {
    LogRecord record(LogRecordType::TxnBegin, txn_id);
    return append(record);
}

Result<Lsn> WriteAheadLog::write_txn_commit(TxnId txn_id) {
    LogRecord record(LogRecordType::TxnCommit, txn_id);
    auto result = append(record);
    
    if (result) {
        // Commit must be durable
        if (auto status = force(*result); !status) {
            return Err<Lsn>(status.error().code(), status.error().message());
        }
    }
    
    return result;
}

Result<Lsn> WriteAheadLog::write_txn_abort(TxnId txn_id) {
    LogRecord record(LogRecordType::TxnAbort, txn_id);
    return append(record);
}

Status WriteAheadLog::rotate_segment() {
    // Flush and close current
    current_segment_.flush();
    current_segment_.close();
    
    // Open new segment
    ++current_segment_id_;
    current_segment_pos_ = 0;
    
    auto path = segment_path(current_segment_id_);
    current_segment_.open(path, std::ios::out | std::ios::binary);
    
    if (!current_segment_.is_open()) {
        return Err(ErrorCode::IoError,
            fmt::format("Failed to create new WAL segment '{}'", path.string()));
    }
    
    return Ok();
}

std::filesystem::path WriteAheadLog::segment_path(std::uint64_t segment_id) const {
    return wal_dir_ / fmt::format("wal_{:08d}.log", segment_id);
}

std::vector<std::uint64_t> WriteAheadLog::find_segments() const {
    std::vector<std::uint64_t> segments;
    
    if (!std::filesystem::exists(wal_dir_)) {
        return segments;
    }
    
    std::regex pattern(R"(wal_(\d{8})\.log)");
    
    for (const auto& entry : std::filesystem::directory_iterator(wal_dir_)) {
        if (!entry.is_regular_file()) continue;
        
        std::smatch match;
        std::string filename = entry.path().filename().string();
        
        if (std::regex_match(filename, match, pattern)) {
            segments.push_back(std::stoull(match[1].str()));
        }
    }
    
    std::sort(segments.begin(), segments.end());
    return segments;
}

} // namespace datyredb::storage
