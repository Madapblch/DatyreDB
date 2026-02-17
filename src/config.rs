use std::time::Duration;

#[derive(Debug, Clone)]
pub struct CheckpointConfig {
    /// Максимальный интервал между checkpoint'ами (fuzzy checkpoint)
    pub max_interval: Duration,
    
    /// Минимальный интервал между checkpoint'ами (защита от checkpoint storm)
    pub min_interval: Duration,
    
    /// Максимальный размер WAL до принудительного checkpoint (в байтах)
    /// Рекомендуется: 1-2 GB для production
    pub max_wal_size: u64,
    
    /// "Мягкий" лимит на размер dirty pages (% от buffer pool)
    /// При достижении начинается фоновый flush
    pub dirty_page_soft_limit_pct: f32,
    
    /// "Жёсткий" лимит на размер dirty pages (% от buffer pool)
    /// При достижении блокируются новые транзакции до завершения checkpoint
    pub dirty_page_hard_limit_pct: f32,
    
    /// Размер буфера для checkpoint write batching (страниц за раз)
    pub checkpoint_batch_size: usize,
    
    /// Использовать ли io_uring для асинхронного checkpoint I/O
    pub async_checkpoint: bool,
}

impl Default for CheckpointConfig {
    fn default() -> Self {
        Self {
            max_interval: Duration::from_secs(60),  // Было 10, делаем консервативнее
            min_interval: Duration::from_secs(5),   // Защита от слишком частых CP
            max_wal_size: 1024 * 1024 * 1024,       // 1 GB WAL
            dirty_page_soft_limit_pct: 0.70,        // 70% buffer pool
            dirty_page_hard_limit_pct: 0.90,        // 90% buffer pool (паника!)
            checkpoint_batch_size: 256,              // 256 страниц = 1MB при 4KB pages
            async_checkpoint: true,
        }
    }
}

#[derive(Debug, Clone)]
pub struct DatabaseConfig {
    pub buffer_pool_size: usize,
    pub page_size: usize,
    pub checkpoint: CheckpointConfig,
    // ... остальные поля
}
