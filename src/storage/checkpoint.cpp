#include "storage/checkpoint.hpp"
#include "utils/logger.hpp"

namespace datyredb::storage {

CheckpointManager::CheckpointManager(
    CheckpointConfig config,
    std::shared_ptr<BufferPool> buffer_pool,
    std::shared_ptr<WriteAheadLog> wal,
    std::shared_ptr<CheckpointMetrics> metrics)
    : config_(std::move(config))
    , buffer_pool_(std::move(buffer_pool))
    , wal_(std::move(wal))
    , metrics_(std::move(metrics))
    , last_checkpoint_time_(std::chrono::steady_clock::now())
{
    Logger::info("CheckpointManager created: "
                 "max_interval={}s, min_interval={}s, "
                 "soft_limit={:.0f}%, hard_limit={:.0f}%",
                 config_.max_interval.count(),
                 config_.min_interval.count(),
                 config_.dirty_page_soft_limit_pct * 100,
                 config_.dirty_page_hard_limit_pct * 100);
}

CheckpointManager::~CheckpointManager() {
    shutdown();
}

void CheckpointManager::start() {
    if (running_.exchange(true)) {
        return;  // Уже запущен
    }
    
    background_thread_ = std::thread(&CheckpointManager::background_loop, this);
    Logger::info("CheckpointManager started");
}

void CheckpointManager::shutdown() {
    if (!running_.exchange(false)) {
        return;  // Уже остановлен
    }
    
    // Будим фоновый поток
    block_cv_.notify_all();
    
    if (background_thread_.joinable()) {
        background_thread_.join();
    }
    
    // Финальный checkpoint
    Logger::info("CheckpointManager: performing shutdown checkpoint");
    do_checkpoint(CheckpointTrigger::Shutdown);
    
    Logger::info("CheckpointManager shutdown complete");
}

void CheckpointManager::manual_checkpoint() {
    std::lock_guard lock(checkpoint_mutex_);
    do_checkpoint(CheckpointTrigger::Manual);
}

bool CheckpointManager::check_pressure() {
    if (!blocking_mode_.load(std::memory_order_relaxed)) {
        return false;
    }
    
    // Ждём завершения blocking checkpoint
    std::unique_lock lock(block_mutex_);
    block_cv_.wait(lock, [this] { 
        return !blocking_mode_.load() || !running_.load(); 
    });
    
    return true;
}

void CheckpointManager::background_loop() {
    while (running_.load()) {
        // Спим секунду между проверками
        {
            std::unique_lock lock(block_mutex_);
            block_cv_.wait_for(lock, std::chrono::seconds(1), [this] {
                return !running_.load();
            });
        }
        
        if (!running_.load()) break;
        
        auto trigger = should_checkpoint();
        
        if (trigger.has_value()) {
            std::lock_guard lock(checkpoint_mutex_);
            do_checkpoint(trigger.value());
        }
    }
}

std::optional<CheckpointTrigger> CheckpointManager::should_checkpoint() const {
    auto now = std::chrono::steady_clock::now();
    auto since_last = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_checkpoint_time_
    );
    
    // =========================================================================
    // 1. HARD LIMIT — критично!
    // =========================================================================
    std::size_t dirty_count = buffer_pool_->dirty_page_count();
    std::size_t capacity = buffer_pool_->capacity();
    float dirty_ratio = static_cast<float>(dirty_count) / static_cast<float>(capacity);
    
    if (dirty_ratio >= config_.dirty_page_hard_limit_pct) {
        Logger::warn("HARD LIMIT: dirty pages {}/{} ({:.1f}%) >= {:.0f}%",
                     dirty_count, capacity, dirty_ratio * 100,
                     config_.dirty_page_hard_limit_pct * 100);
        return CheckpointTrigger::DirtyHardLimit;
    }
    
    // =========================================================================
    // 2. Минимальный интервал
    // =========================================================================
    if (since_last < config_.min_interval) {
        return std::nullopt;
    }
    
    // =========================================================================
    // 3. WAL size
    // =========================================================================
    uint64_t wal_size = wal_->current_size();
    if (wal_size >= config_.max_wal_size) {
        Logger::info("WAL size {} >= {} threshold",
                     wal_size, config_.max_wal_size);
        return CheckpointTrigger::WalSize;
    }
    
    // =========================================================================
    // 4. Soft limit
    // =========================================================================
    if (dirty_ratio >= config_.dirty_page_soft_limit_pct) {
        Logger::debug("Soft limit: dirty pages {}/{} ({:.1f}%)",
                      dirty_count, capacity, dirty_ratio * 100);
        return CheckpointTrigger::DirtySoftLimit;
    }
    
    // =========================================================================
    // 5. Timer
    // =========================================================================
    if (since_last >= config_.max_interval) {
        return CheckpointTrigger::Timer;
    }
    
    return std::nullopt;
}

void CheckpointManager::do_checkpoint(CheckpointTrigger trigger) {
    auto start_time = std::chrono::steady_clock::now();
    
    checkpoint_in_progress_ = true;
    
    // При hard limit — блокируем новые транзакции
    if (trigger == CheckpointTrigger::DirtyHardLimit) {
        blocking_mode_ = true;
        metrics_->blocking_checkpoint_count.fetch_add(1);
        Logger::warn("BLOCKING CHECKPOINT: transactions will wait");
    }
    
    const char* trigger_name = checkpoint_trigger_name(trigger);
    
    Logger::info("Checkpoint BEGIN (trigger={})", trigger_name);
    
    // =========================================================================
    // ФАЗА 1: BEGIN CHECKPOINT в WAL
    // =========================================================================
    Lsn begin_lsn = wal_->write_checkpoint_begin();
    
    // =========================================================================
    // ФАЗА 2: Snapshot dirty pages
    // =========================================================================
    auto dirty_pages = buffer_pool_->get_dirty_pages();
    std::size_t total_pages = dirty_pages.size();
    
    if (total_pages == 0) {
        Logger::info("Checkpoint: no dirty pages");
        wal_->write_checkpoint_end(begin_lsn);
        checkpoint_in_progress_ = false;
        blocking_mode_ = false;
        block_cv_.notify_all();
        last_checkpoint_time_ = std::chrono::steady_clock::now();
        return;
    }
    
    // =========================================================================
    // ФАЗА 3: Flush dirty pages батчами
    // =========================================================================
    std::size_t pages_written = 0;
    std::size_t batch_size = config_.checkpoint_batch_size;
    
    for (std::size_t i = 0; i < dirty_pages.size(); i += batch_size) {
        if (!running_.load() && trigger != CheckpointTrigger::Shutdown) {
            Logger::warn("Checkpoint interrupted");
            break;
        }
        
        std::size_t end = std::min(i + batch_size, dirty_pages.size());
        std::vector<PageId> batch(dirty_pages.begin() + i, 
                                   dirty_pages.begin() + end);
        
        if (!buffer_pool_->flush_pages(batch)) {
            Logger::error("Checkpoint: failed to flush batch at {}", i);
            continue;
        }
        
        pages_written += batch.size();
        
        // Throttling для soft limit
        if (trigger == CheckpointTrigger::DirtySoftLimit) {
            std::this_thread::sleep_for(config_.batch_throttle_us);
        }
        
        // Progress log каждые 25%
        std::size_t quarter = total_pages / 4;
        if (quarter > 0 && pages_written % quarter == 0) {
            Logger::debug("Checkpoint progress: {}/{} ({:.0f}%)",
                          pages_written, total_pages,
                          100.0 * pages_written / total_pages);
        }
    }
    
    // =========================================================================
    // ФАЗА 4: Sync
    // =========================================================================
    buffer_pool_->sync_all();
    
    // =========================================================================
    // ФАЗА 5: END CHECKPOINT
    // =========================================================================
    Lsn end_lsn = wal_->write_checkpoint_end(begin_lsn);
    
    // =========================================================================
    // ФАЗА 6: Truncate WAL
    // =========================================================================
    wal_->truncate_before(begin_lsn);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    // Метрики
    bool was_forced = (trigger != CheckpointTrigger::Timer && 
                       trigger != CheckpointTrigger::Manual);
    metrics_->record_checkpoint(duration, pages_written, was_forced);
    
    Logger::info("Checkpoint END (trigger={}, pages={}/{}, duration={}ms, LSN={})",
                 trigger_name, pages_written, total_pages, 
                 duration.count(), end_lsn);
    
    // Снимаем блокировку
    checkpoint_in_progress_ = false;
    blocking_mode_ = false;
    block_cv_.notify_all();
    
    last_checkpoint_time_ = end_time;
}

} // namespace datyredb::storage
