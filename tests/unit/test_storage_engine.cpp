// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Storage Engine Unit Tests                                        ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include <gtest/gtest.h>

#include "internal/core/storage_engine.hpp"

#include <filesystem>

using namespace datyredb::core;
using namespace datyredb::storage;

class StorageEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "datyredb_engine_test";
        std::filesystem::remove_all(test_dir_);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }
    
    std::filesystem::path test_dir_;
};

// ==============================================================================
// Initialization
// ==============================================================================

TEST_F(StorageEngineTest, DefaultConstruction) {
    StorageEngine engine;
    EXPECT_FALSE(engine.is_initialized());
}

TEST_F(StorageEngineTest, Initialize) {
    StorageEngine::Config config;
    config.data_directory = test_dir_.string();
    config.buffer_pool_size = 100;
    
    StorageEngine engine(config);
    
    auto status = engine.initialize();
    EXPECT_TRUE(status.has_value());
    EXPECT_TRUE(engine.is_initialized());
    
    engine.shutdown();
    EXPECT_FALSE(engine.is_initialized());
}

TEST_F(StorageEngineTest, DoubleInitialize) {
    StorageEngine::Config config;
    config.data_directory = test_dir_.string();
    
    StorageEngine engine(config);
    
    EXPECT_TRUE(engine.initialize().has_value());
    EXPECT_TRUE(engine.initialize().has_value());  // Should be OK
    
    engine.shutdown();
}

// ==============================================================================
// Page Operations
// ==============================================================================

TEST_F(StorageEngineTest, CreatePage) {
    StorageEngine::Config config;
    config.data_directory = test_dir_.string();
    config.buffer_pool_size = 100;
    
    StorageEngine engine(config);
    ASSERT_TRUE(engine.initialize().has_value());
    
    auto result = engine.create_page();
    ASSERT_TRUE(result.has_value());
    
    Page* page = *result;
    EXPECT_NE(page, nullptr);
    EXPECT_NE(page->id(), INVALID_PAGE_ID);
    
    EXPECT_TRUE(engine.release_page(page->id(), false).has_value());
    
    engine.shutdown();
}

TEST_F(StorageEngineTest, GetPage) {
    StorageEngine::Config config;
    config.data_directory = test_dir_.string();
    config.buffer_pool_size = 100;
    
    StorageEngine engine(config);
    ASSERT_TRUE(engine.initialize().has_value());
    
    // Create page
    auto create_result = engine.create_page();
    ASSERT_TRUE(create_result.has_value());
    
    PageId page_id = (*create_result)->id();
    
    // Write data
    std::memcpy((*create_result)->payload(), "TestData", 9);
    
    EXPECT_TRUE(engine.release_page(page_id, true).has_value());
    
    // Get page again
    auto get_result = engine.get_page(page_id);
    ASSERT_TRUE(get_result.has_value());
    
    EXPECT_STREQ((*get_result)->payload(), "TestData");
    
    EXPECT_TRUE(engine.release_page(page_id, false).has_value());
    
    engine.shutdown();
}

// ==============================================================================
// Checkpoint
// ==============================================================================

TEST_F(StorageEngineTest, ManualCheckpoint) {
    StorageEngine::Config config;
    config.data_directory = test_dir_.string();
    config.buffer_pool_size = 100;
    
    StorageEngine engine(config);
    ASSERT_TRUE(engine.initialize().has_value());
    
    // Create some dirty pages
    for (int i = 0; i < 10; ++i) {
        auto result = engine.create_page();
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(engine.release_page((*result)->id(), true).has_value());
    }
    
    EXPECT_GT(engine.dirty_page_count(), 0);
    
    auto status = engine.checkpoint_sync();
    EXPECT_TRUE(status.has_value());
    
    EXPECT_EQ(engine.dirty_page_count(), 0);
    
    engine.shutdown();
}

// ==============================================================================
// Statistics
// ==============================================================================

TEST_F(StorageEngineTest, Statistics) {
    StorageEngine::Config config;
    config.data_directory = test_dir_.string();
    config.buffer_pool_size = 50;
    
    StorageEngine engine(config);
    ASSERT_TRUE(engine.initialize().has_value());
    
    EXPECT_EQ(engine.buffer_pool_size(), 0);
    EXPECT_EQ(engine.dirty_page_count(), 0);
    EXPECT_EQ(engine.page_count(), 0);
    
    auto result = engine.create_page();
    ASSERT_TRUE(result.has_value());
    
    EXPECT_EQ(engine.buffer_pool_size(), 1);
    EXPECT_EQ(engine.page_count(), 1);
    
    EXPECT_TRUE(engine.release_page((*result)->id(), true).has_value());
    
    EXPECT_EQ(engine.dirty_page_count(), 1);
    
    engine.shutdown();
}

TEST_F(StorageEngineTest, Metrics) {
    StorageEngine::Config config;
    config.data_directory = test_dir_.string();
    
    StorageEngine engine(config);
    ASSERT_TRUE(engine.initialize().has_value());
    
    const auto* metrics = engine.metrics();
    EXPECT_NE(metrics, nullptr);
    EXPECT_EQ(metrics->checkpoint_count.load(), 0);
    
    engine.shutdown();
    
    // Shutdown checkpoint should have occurred
    EXPECT_GT(metrics->checkpoint_count.load(), 0);
}

// ==============================================================================
// Persistence
// ==============================================================================

TEST_F(StorageEngineTest, DataPersistence) {
    PageId page_id;
    
    // First session - create and write
    {
        StorageEngine::Config config;
        config.data_directory = test_dir_.string();
        config.buffer_pool_size = 100;
        
        StorageEngine engine(config);
        ASSERT_TRUE(engine.initialize().has_value());
        
        auto result = engine.create_page();
        ASSERT_TRUE(result.has_value());
        
        page_id = (*result)->id();
        std::memcpy((*result)->payload(), "PersistentData", 15);
        
        EXPECT_TRUE(engine.release_page(page_id, true).has_value());
        
        engine.shutdown();
    }
    
    // Second session - read
    {
        StorageEngine::Config config;
        config.data_directory = test_dir_.string();
        config.buffer_pool_size = 100;
        
        StorageEngine engine(config);
        ASSERT_TRUE(engine.initialize().has_value());
        
        auto result = engine.get_page(page_id);
        ASSERT_TRUE(result.has_value());
        
        EXPECT_STREQ((*result)->payload(), "PersistentData");
        
        EXPECT_TRUE(engine.release_page(page_id, false).has_value());
        
        engine.shutdown();
    }
}
