#include "storage/wal.hpp"
#include "utils/logger.hpp"

#include <cstring>
#include <algorithm>

namespace datyredb::storage {

// ============================================================================
// LogRecord
// ============================================================================

std::size_t LogRecord::serialized_size() const {
    return sizeof(LogRecordType)    // type
         + sizeof(Lsn)              // lsn
         + sizeof(TxnId)            // txn_id
         + sizeof(PageId)           // page_id
         + sizeof(uint16_t)         // offset
         + sizeof(uint16_t)         // length
         + sizeof(Lsn)              // prev_lsn
         + sizeof(uint32_t)         // data size
         + data.size();             // data
}

void LogRecord::serialize(std::vector<char>& buffer) const {
    buffer.resize(serialized_size());
    char* ptr = buffer.data();
    
    std::memcpy(ptr, &type, sizeof(type)); ptr += sizeof(type);
    std::memcpy(ptr, &lsn, sizeof(lsn)); ptr += sizeof(lsn);
    std::memcpy(ptr, &txn_id, sizeof(txn_id)); ptr += sizeof(txn_id);
    std::memcpy(ptr, &page_id, sizeof(page_id)); ptr += sizeof(page_id);
    std::memcpy(ptr, &offset, sizeof(offset)); ptr += sizeof(offset);
    std::memcpy(ptr, &length, sizeof(length)); ptr += sizeof(length);
    std::memcpy(ptr, &prev_lsn, sizeof(prev_lsn)); ptr += sizeof(prev_lsn);
    
    uint32_t data_size = static_cast<uint32_t>(data.size());
    std::memcpy(ptr, &data_size, sizeof(data_size)); ptr += sizeof(data_size);
    
    if (!data.empty()) {
        std::memcpy(ptr, data.data(), data.size());
    }
}

LogRecord LogRecord::deserialize(const char* buf, std::size_t size) {
    LogRecord rec;
    const char* ptr = buf;
    
    std::memcpy(&rec.type, ptr, sizeof(rec.type)); ptr += sizeof(rec.type);
    std::memcpy(&rec.lsn, ptr, sizeof(rec.lsn)); ptr += sizeof(rec.lsn);
    std::memcpy(&rec.txn_id, ptr, sizeof(rec.txn_id)); ptr += sizeof(rec.txn_id);
    std::memcpy(&rec.page_id, ptr, sizeof(rec.page_id)); ptr += sizeof(rec.page_id);
    std::memcpy(&rec.offset, ptr, sizeof(rec.offset)); ptr += sizeof(rec.offset);
    std::memcpy(&rec.length, ptr, sizeof(rec.length)); ptr += sizeof(rec.length);
    std::memcpy(&rec.prev_lsn, ptr, sizeof(rec.prev_lsn)); ptr += sizeof(rec.prev_lsn);
    
    uint32_t data_size;
    std::memcpy(&data_size, ptr, sizeof(data_size)); ptr += sizeof(data_size);
    
    if (data_size > 0) {
        rec.data.resize(data_size);
        std::memcpy(rec.data.data(), ptr, data_size);
    }
    
    return rec;
}

// ============================================================================
// WriteAheadLog
// ============================================================================

WriteAheadLog::WriteAheadLog(const std::filesystem::path& wal_dir,
                             std::size_t segment_size,
                             std::shared_ptr<CheckpointMetrics> metrics)
    : wal_dir_(wal_dir)
    , segment_size_(segment_size)
    , metrics_(std::move(metrics))
{
}

WriteAheadLog::~WriteAheadLog() {
    shutdown();
}

bool WriteAheadLog::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Создаём директорию
    std::error_code ec;
    std::filesystem::create_directories(wal_dir_, ec);
    if (ec) {
        Logger::error("WAL: failed to create directory {}: {}",
                      wal_dir_.string(), ec.message());
        return false;
    }
    
    // Находим последний сегмент
    uint64_t max_segment = 0;
    for (const auto& entry : std::filesystem::directory_iterator(wal_dir_)) {
        auto filename = entry.path().filename().string();
        if (filename.find("wal_") == 0 && filename.size() > 4) {
            try {
                uint64_t seg_id = std::stoull(filename.substr(4));
                max_segment = std::max(max_segment, seg_id);
            } catch (...) {
                continue;
            }
        }
    }
    
    current_segment_id_ = max_segment;
    
    // Открываем текущий сегмент
    auto path = segment_path(current_segment_id_);
    current_segment_.open(path, 
                          std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
    
    if (!current_segment_.is_open()) {
        // Создаём новый
        current_segment_.open(path, std::ios::out | std::ios::binary);
        if (!current_segment_.is_open()) {
            Logger::error("WAL: failed to create segment {}", path.string());
            return false;
        }
        current_segment_.close();
        current_segment_.open(path,
                              std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
    }
    
    // Определяем позицию в сегменте
    current_segment_.seekp(0, std::ios::end);
    current_segment_pos_ = static_cast<std::size_t>(current_segment_.tellp());
    
    // Вычисляем общий размер WAL
    uint64_t total_size = 0;
    for (const auto& entry : std::filesystem::directory_iterator(wal_dir_)) {
        if (entry.is_regular_file()) {
            total_size += entry.file_size();
        }
    }
    current_size_.store(total_size);
    metrics_->current_wal_size.store(total_size);
    
    initialized_ = true;
    
    Logger::info("WAL initialized: dir={}, segments={}, size={} bytes",
                 wal_dir_.string(),
                 current_segment_id_ + 1,
                 total_size);
    
    return true;
}

void WriteAheadLog::shutdown() {
    if (!initialized_) {
        return;
    }
    
    std::lock_guard lock(append_mutex_);
    
    if (current_segment_.is_open()) {
        current_segment_.flush();
        current_segment_.close();
    }
    
    initialized_ = false;
    Logger::info("WAL shutdown");
}

Lsn WriteAheadLog::append(const LogRecord& record) {
    std::lock_guard lock(append_mutex_);
    
    LogRecord rec = record;
    rec.lsn = next_lsn_.fetch_add(1);
    
    std::vector<char> buffer;
    rec.serialize(buffer);
    
    // Проверяем, нужен ли новый сегмент
    if (current_segment_pos_ + buffer.size() > segment_size_) {
        if (!rotate_segment()) {
            Logger::error("WAL: failed to rotate segment");
            return INVALID_LSN;
        }
    }
    
    // Пишем запись
    current_segment_.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    current_segment_pos_ += buffer.size();
    
    uint64_t new_size = current_size_.fetch_add(buffer.size()) + buffer.size();
    metrics_->current_wal_size.store(new_size);
    
    return rec.lsn;
}

void WriteAheadLog::force(Lsn lsn) {
    std::lock_guard lock(append_mutex_);
    current_segment_.flush();
    flushed_lsn_.store(lsn);
}

Lsn WriteAheadLog::write_checkpoint_begin() {
    LogRecord rec;
    rec.type = LogRecordType::CHECKPOINT_BEGIN;
    rec.txn_id = 0;
    rec.page_id = INVALID_PAGE_ID;
    
    Lsn lsn = append(rec);
    Logger::debug("WAL: CHECKPOINT_BEGIN at LSN {}", lsn);
    return lsn;
}

Lsn WriteAheadLog::write_checkpoint_end(Lsn begin_lsn) {
    LogRecord rec;
    rec.type = LogRecordType::CHECKPOINT_END;
    rec.txn_id = 0;
    rec.page_id = INVALID_PAGE_ID;
    rec.prev_lsn = begin_lsn;
    
    Lsn lsn = append(rec);
    force(lsn);
    
    Logger::debug("WAL: CHECKPOINT_END at LSN {} (begin={})", lsn, begin_lsn);
    return lsn;
}

void WriteAheadLog::truncate_before(Lsn lsn) {
    std::lock_guard lock(append_mutex_);
    
    // Удаляем старые сегменты (упрощённая логика)
    // В production нужно отслеживать LSN в каждом сегменте
    
    uint64_t freed = 0;
    
    for (const auto& entry : std::filesystem::directory_iterator(wal_dir_)) {
        auto filename = entry.path().filename().string();
        if (filename.find("wal_") != 0) continue;
        
        try {
            uint64_t seg_id = std::stoull(filename.substr(4));
            // Удаляем сегменты кроме текущего и предыдущего
            if (seg_id + 2 <= current_segment_id_) {
                freed += entry.file_size();
                std::filesystem::remove(entry.path());
                Logger::debug("WAL: removed old segment {}", filename);
            }
        } catch (...) {
            continue;
        }
    }
    
    if (freed > 0) {
        current_size_.fetch_sub(freed);
        metrics_->current_wal_size.store(current_size_.load());
        Logger::info("WAL: truncated {} bytes", freed);
    }
}

bool WriteAheadLog::rotate_segment() {
    current_segment_.flush();
    current_segment_.close();
    
    ++current_segment_id_;
    current_segment_pos_ = 0;
    
    auto path = segment_path(current_segment_id_);
    current_segment_.open(path, std::ios::out | std::ios::binary);
    
    if (!current_segment_.is_open()) {
        Logger::error("WAL: failed to create new segment {}", path.string());
        return false;
    }
    
    Logger::debug("WAL: rotated to segment {}", current_segment_id_);
    return true;
}

std::filesystem::path WriteAheadLog::segment_path(uint64_t segment_id) const {
    return wal_dir_ / ("wal_" + std::to_string(segment_id));
}

} // namespace datyredb::storage
