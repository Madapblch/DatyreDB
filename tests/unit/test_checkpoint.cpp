// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Checkpoint Unit Tests                                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include <gtest/gtest.h>

#include "internal/storage/checkpoint.hpp"
#include "internal/storage/buffer_pool.hpp"
#include "internal/storage/wal.hpp"
#include "internal/storage/disk_manager.hpp"

#include <filesystem>
#include <thread>
#include <chrono>

using namespace datyredb::storage;

class CheckpointTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "datyredb_cp_test";
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
        
        metrics_ = std::make_shared<CheckpointMetrics>();
        
        disk_manager_ = std::make_shared<DiskManager>(test_dir_);
        ASSERT_TRUE(disk_manager_->open().has_value());
        
        wal_ = std::make_shared<WriteAheadLog>(test_dir_ / "wal", 1024 * 1024, metrics_);
        ASSERT_TRUE(wal_->open().has_value());
        
        buffer_pool_ = std::make_shared<BufferPool>(100, disk_manager_, metrics_);
    }
    
    void TearDown() override {
        if (checkpoint_manager_) {
            checkpoint_manager_->stop();
            checkpoint_manager_.reset();
        }
        buffer_pool_.reset();
        wal_->close();
        wal_.reset();
        disk_manager_->close();
        disk_manager_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    void create_checkpoint_manager(CheckpointConfig config = {}) {
        checkpoint_manager_ = std::make_shared<CheckpointManager>(
            config, buffer_pool_, wal_, metrics_
        );
    }
    
    void create_dirty_pages(int count) {
        for (int i = 0; i < count; ++i) {
            auto result = buffer_pool_->new_page();
            ASSERT_TRUE(result.has_value());
            ASSERT_TRUE(buffer_pool_->unpin_page((*result)->id(), true).has_value());
        }
    }
    
    std::filesystem::path test_dir_;
    std::shared_ptr<CheckpointMetrics> metrics_;
    std::shared_ptr<DiskManager> disk_manager_;
    std::shared_ptr<WriteAheadLog> wal_;
    std::shared_ptr<BufferPool> buffer_pool_;
    std::shared_ptr<CheckpointManager> checkpoint_manager_;
};

// ==============================================================================
// Manual Checkpoint
// ==============================================================================

TEST_F(CheckpointTest, ManualCheckpointSync) {
    create_checkpoint_manager();
    
    create_dirty_pages(10);
    EXPECT_EQ(buffer_pool_->dirty_count(), 10);
    EXPECT_EQ(metrics_->checkpoint_count.load(), 0);
    
    auto status = checkpoint_manager_->checkpoint_sync();
    EXPECT_TRUE(status.has_value());
    
    EXPECT_EQ(buffer_pool_->dirty_count(), 0);
    EXPECT_EQ(metrics_->checkpoint_count.load(), 1);
    EXPECT_GT(metrics_->pages_written.load(), 0);
}

TEST_F(CheckpointTest, ManualCheckpointNoDirtyPages) {
    create_checkpoint_manager();
    
    // No dirty pages
    EXPECT_EQ(buffer_pool_->dirty_count(), 0);
    
    auto status = checkpoint_manager_->checkpoint_sync();
    EXPECT_TRUE(status.has_value());
    
    EXPECT_EQ(metrics_->checkpoint_count.load(), 1);
    EXPECT_EQ(metrics_->pages_written.load(), 0);
}

// ==============================================================================
// Background Checkpoint
// ==============================================================================

TEST_F(CheckpointTest, BackgroundCheckpointTimer) {
    CheckpointConfig config;
    config.max_interval = std::chrono::seconds(1);
    config.min_interval = std::chrono::seconds(0);
    
    create_checkpoint_manager(config);
    checkpoint_manager_->start();
    
    create_dirty_pages(5);
    
    // Wait for timer checkpoint
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    EXPECT_GT(metrics_->checkpoint_count.load(), 0);
}

TEST_F(CheckpointTest, BackgroundCheckpointRequest) {
    CheckpointConfig config;
    config.max_interval = std::chrono::seconds(300);  // Disable timer
    config.min_interval = std::chrono::seconds(0);
    
    create_checkpoint_manager(config);
    checkpoint_manager_->start();
    
    create_dirty_pages(5);
    
    checkpoint_manager_->request_checkpoint();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_GT(metrics_->checkpoint_count.load(), 0);
}

// ==============================================================================
// Soft Limit Trigger
// ==============================================================================

TEST_F(CheckpointTest, SoftLimitTrigger) {
    CheckpointConfig config;
    config.dirty_soft_limit = 0.10f;  // 10% = 10 pages (pool size 100)
    config.min_interval = std::chrono::seconds(0);
    config.max_interval = std::chrono::seconds(300);
    
    create_checkpoint_manager(config);
    checkpoint_manager_->start();
    
    // Create more than 10% dirty pages
    create_dirty_pages(15);
    
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    EXPECT_GT(metrics_->checkpoint_count.load(), 0);
    EXPECT_LT(buffer_pool_->dirty_count(), 15);
}

// ==============================================================================
// Metrics
// ==============================================================================

TEST_F(CheckpointTest, Metrics) {
    create_checkpoint_manager();
    
    create_dirty_pages(20);
    
    auto status = checkpoint_manager_->checkpoint_sync();
    EXPECT_TRUE(status.has_value());
    
    EXPECT_EQ(metrics_->checkpoint_count.load(), 1);
    EXPECT_GE(metrics_->pages_written.load(), 20);
    EXPECT_GT(metrics_->total_duration_us.load(), 0);
    
    auto avg_duration = metrics_->average_duration();
    EXPECT_GT(avg_duration.count(), 0);
}

// ==============================================================================
// Shutdown
// ==============================================================================

TEST_F(CheckpointTest, ShutdownCheckpoint) {
    create_checkpoint_manager();
    checkpoint_manager_->start();
    
    create_dirty_pages(10);
    EXPECT_EQ(buffer_pool_->dirty_count(), 10);
    
    // Stop should do final checkpoint
    checkpoint_manager_->stop();
    
    EXPECT_EQ(buffer_pool_->dirty_count(), 0);
    EXPECT_GT(metrics_->checkpoint_count.load(), 0);
}

// ==============================================================================
// Concurrent Access
// ==============================================================================

TEST_F(CheckpointTest, ConcurrentPageCreation) {
    CheckpointConfig config;
    config.max_interval = std::chrono::seconds(1);
    config.min_interval = std::chrono::seconds(0); // Using 0 for immediate effect
    
    create_checkpoint_manager(config);
    checkpoint_manager_->start();
    
    std::atomic<int> pages_created{0};
    
    // Multiple threads creating pages
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([this, &pages_created]() {
            for (int i = 0; i < 20; ++i) {
                auto result = buffer_pool_->new_page();
                if (result.has_value()) {
                    (void)buffer_pool_->unpin_page((*result)->id(), true);
                    pages_created.fetch_add(1);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_GT(pages_created.load(), 0);
    
    // Final checkpoint
    checkpoint_manager_->stop();
    
    EXPECT_EQ(buffer_pool_->dirty_count(), 0);
}
