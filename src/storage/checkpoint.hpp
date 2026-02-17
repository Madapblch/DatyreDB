#pragma once

#include "storage/storage_types.hpp"
#include "storage/buffer_pool.hpp"
#include "storage/wal.hpp"

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>

namespace datyredb::storage {

/// Production-grade Checkpoint Manager
class CheckpointManager {
public:
    CheckpointManager(CheckpointConfig config,
                      std::shared_ptr<BufferPool> buffer_pool,
                      std::shared_ptr<WriteAheadLog> wal,
                      std::shared_ptr<CheckpointMetrics> metrics);
    ~CheckpointManager();
    
    // Запретить копирование
    CheckpointManager(const CheckpointManager&) = delete;
    CheckpointManager& operator=(const CheckpointManager&) = delete;
    
    /// Запуск фонового потока
    void start();
    
    /// Остановка
    void shutdown();
    
    /// Ручной checkpoint
    void manual_checkpoint();
    
    /// Проверка давления (вызывается перед транзакцией)
    /// Возвращает true если транзакция должна подождать
    bool check_pressure();
    
    /// Запущен ли менеджер
    bool is_running() const { 
        return running_.load(std::memory_order_relaxed); 
    }
    
private:
    /// Фоновый поток
    void background_loop();
    
    /// Проверка триггеров
    std::optional<CheckpointTrigger> should_checkpoint() const;
    
    /// Выполнение checkpoint
    void do_checkpoint(CheckpointTrigger trigger);
    
    CheckpointConfig config_;
    std::shared_ptr<BufferPool> buffer_pool_;
    std::shared_ptr<WriteAheadLog> wal_;
    std::shared_ptr<CheckpointMetrics> metrics_;
    
    std::thread background_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> checkpoint_in_progress_{false};
    
    // Для блокировки при hard limit
    std::mutex block_mutex_;
    std::condition_variable block_cv_;
    std::atomic<bool> blocking_mode_{false};
    
    std::chrono::steady_clock::time_point last_checkpoint_time_;
    
    std::mutex checkpoint_mutex_;
};

} // namespace datyredb::storage
