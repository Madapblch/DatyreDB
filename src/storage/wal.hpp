#pragma once

#include "storage/storage_types.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include <atomic>
#include <memory>

namespace datyredb::storage {

/// WAL запись
struct LogRecord {
    LogRecordType type = LogRecordType::INVALID;
    Lsn lsn = INVALID_LSN;
    TxnId txn_id = 0;
    PageId page_id = INVALID_PAGE_ID;
    uint16_t offset = 0;
    uint16_t length = 0;
    Lsn prev_lsn = INVALID_LSN;
    std::vector<char> data;
    
    /// Размер записи при сериализации
    std::size_t serialized_size() const;
    
    /// Сериализация в буфер
    void serialize(std::vector<char>& buffer) const;
    
    /// Десериализация
    static LogRecord deserialize(const char* data, std::size_t size);
};

/// Write-Ahead Log
class WriteAheadLog {
public:
    WriteAheadLog(const std::filesystem::path& wal_dir,
                  std::size_t segment_size,
                  std::shared_ptr<CheckpointMetrics> metrics);
    ~WriteAheadLog();
    
    // Запретить копирование
    WriteAheadLog(const WriteAheadLog&) = delete;
    WriteAheadLog& operator=(const WriteAheadLog&) = delete;
    
    /// Инициализация
    bool initialize();
    
    /// Закрытие
    void shutdown();
    
    /// Записать лог запись
    Lsn append(const LogRecord& record);
    
    /// Force WAL до указанного LSN
    void force(Lsn lsn);
    
    /// Checkpoint BEGIN
    Lsn write_checkpoint_begin();
    
    /// Checkpoint END
    Lsn write_checkpoint_end(Lsn begin_lsn);
    
    /// Обрезка WAL
    void truncate_before(Lsn lsn);
    
    /// Текущий размер WAL
    uint64_t current_size() const { 
        return current_size_.load(std::memory_order_relaxed); 
    }
    
    /// Текущий LSN
    Lsn current_lsn() const { 
        return next_lsn_.load(std::memory_order_relaxed); 
    }
    
    /// Flushed LSN
    Lsn flushed_lsn() const {
        return flushed_lsn_.load(std::memory_order_relaxed);
    }
    
private:
    /// Переход к новому сегменту
    bool rotate_segment();
    
    /// Путь к сегменту
    std::filesystem::path segment_path(uint64_t segment_id) const;
    
    std::filesystem::path wal_dir_;
    std::size_t segment_size_;
    std::shared_ptr<CheckpointMetrics> metrics_;
    
    std::fstream current_segment_;
    uint64_t current_segment_id_ = 0;
    std::size_t current_segment_pos_ = 0;
    
    std::atomic<Lsn> next_lsn_{1};
    std::atomic<Lsn> flushed_lsn_{0};
    std::atomic<uint64_t> current_size_{0};
    
    std::mutex append_mutex_;
    bool initialized_ = false;
};

} // namespace datyredb::storage
