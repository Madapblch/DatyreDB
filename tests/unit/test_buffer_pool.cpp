// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Buffer Pool Unit Tests                                           ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include <gtest/gtest.h>

#include "internal/storage/buffer_pool.hpp"
#include "internal/storage/disk_manager.hpp"

#include <filesystem>
#include <cstring>

using namespace datyredb::storage;

class BufferPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "datyredb_bp_test";
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
        
        metrics_ = std::make_shared<CheckpointMetrics>();
        disk_manager_ = std::make_shared<DiskManager>(test_dir_);
        ASSERT_TRUE(disk_manager_->open().has_value());
        
        buffer_pool_ = std::make_shared<BufferPool>(10, disk_manager_, metrics_);
    }
    
    void TearDown() override {
        buffer_pool_.reset();
        disk_manager_->close();
        disk_manager_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    std::filesystem::path test_dir_;
    std::shared_ptr<CheckpointMetrics> metrics_;
    std::shared_ptr<DiskManager> disk_manager_;
    std::shared_ptr<BufferPool> buffer_pool_;
};

// ==============================================================================
// Basic Operations
// ==============================================================================

TEST_F(BufferPoolTest, NewPage) {
    auto result = buffer_pool_->new_page();
    ASSERT_TRUE(result.has_value());
    
    Page* page = *result;
    EXPECT_NE(page, nullptr);
    EXPECT_NE(page->id(), INVALID_PAGE_ID);
    EXPECT_TRUE(page->is_pinned());
    EXPECT_EQ(page->pin_count(), 1);
    
    EXPECT_TRUE(buffer_pool_->unpin_page(page->id(), false).has_value());
}

TEST_F(BufferPoolTest, CreateMultiplePages) {
    std::vector<PageId> page_ids;
    
    for (int i = 0; i < 5; ++i) {
        auto result = buffer_pool_->new_page();
        ASSERT_TRUE(result.has_value());
        
        Page* page = *result;
        page_ids.push_back(page->id());
        
        // Write unique data
        std::memset(page->payload(), static_cast<char>(i), 100);
        
        EXPECT_TRUE(buffer_pool_->unpin_page(page->id(), true).has_value());
    }
    
    // Verify all pages have unique IDs
    for (std::size_t i = 0; i < page_ids.size(); ++i) {
        for (std::size_t j = i + 1; j < page_ids.size(); ++j) {
            EXPECT_NE(page_ids[i], page_ids[j]);
        }
    }
}

TEST_F(BufferPoolTest, FetchPage) {
    // Create page
    auto create_result = buffer_pool_->new_page();
    ASSERT_TRUE(create_result.has_value());
    
    Page* page = *create_result;
    PageId page_id = page->id();
    
    // Write data
    const char* test_data = "TestData12345";
    std::memcpy(page->payload(), test_data, std::strlen(test_data) + 1);
    
    EXPECT_TRUE(buffer_pool_->unpin_page(page_id, true).has_value());
    EXPECT_TRUE(buffer_pool_->flush_page(page_id).has_value());
    
    // Fetch again
    auto fetch_result = buffer_pool_->fetch_page(page_id);
    ASSERT_TRUE(fetch_result.has_value());
    
    Page* fetched = *fetch_result;
    EXPECT_EQ(fetched->id(), page_id);
    EXPECT_STREQ(fetched->payload(), test_data);
    
    EXPECT_TRUE(buffer_pool_->unpin_page(page_id, false).has_value());
}

// ==============================================================================
// Dirty Page Tracking
// ==============================================================================

TEST_F(BufferPoolTest, DirtyPageTracking) {
    EXPECT_EQ(buffer_pool_->dirty_count(), 0);
    
    auto result = buffer_pool_->new_page();
    ASSERT_TRUE(result.has_value());
    
    Page* page = *result;
    PageId page_id = page->id();
    
    // Not dirty yet
    EXPECT_TRUE(buffer_pool_->unpin_page(page_id, false).has_value());
    EXPECT_EQ(buffer_pool_->dirty_count(), 0);
    
    // Fetch and mark dirty
    auto fetch_result = buffer_pool_->fetch_page(page_id);
    ASSERT_TRUE(fetch_result.has_value());
    
    EXPECT_TRUE(buffer_pool_->unpin_page(page_id, true).has_value());
    EXPECT_EQ(buffer_pool_->dirty_count(), 1);
    
    // Flush clears dirty
    EXPECT_TRUE(buffer_pool_->flush_page(page_id).has_value());
    EXPECT_EQ(buffer_pool_->dirty_count(), 0);
}

TEST_F(BufferPoolTest, GetDirtyPages) {
    std::vector<PageId> expected_dirty;
    
    for (int i = 0; i < 5; ++i) {
        auto result = buffer_pool_->new_page();
        ASSERT_TRUE(result.has_value());
        
        Page* page = *result;
        PageId page_id = page->id();
        
        // Mark some dirty
        bool dirty = (i % 2 == 0);
        if (dirty) {
            expected_dirty.push_back(page_id);
        }
        
        EXPECT_TRUE(buffer_pool_->unpin_page(page_id, dirty).has_value());
    }
    
    auto dirty_pages = buffer_pool_->get_dirty_pages();
    EXPECT_EQ(dirty_pages.size(), expected_dirty.size());
    
    for (PageId id : expected_dirty) {
        EXPECT_NE(std::find(dirty_pages.begin(), dirty_pages.end(), id), dirty_pages.end());
    }
}

// ==============================================================================
// Eviction
// ==============================================================================

TEST_F(BufferPoolTest, EvictionWhenFull) {
    // Pool size is 10, create 15 pages
    std::vector<PageId> all_pages;
    
    for (int i = 0; i < 15; ++i) {
        auto result = buffer_pool_->new_page();
        ASSERT_TRUE(result.has_value());
        
        Page* page = *result;
        all_pages.push_back(page->id());
        
        // Write identifier
        page->payload()[0] = static_cast<char>(i);
        
        EXPECT_TRUE(buffer_pool_->unpin_page(page->id(), true).has_value());
    }
    
    // All pages should still be accessible (loaded from disk if evicted)
    for (std::size_t i = 0; i < 15; ++i) {
        auto result = buffer_pool_->fetch_page(all_pages[i]);
        ASSERT_TRUE(result.has_value());
        
        Page* page = *result;
        EXPECT_EQ(static_cast<int>(page->payload()[0]), static_cast<int>(i));
        
        EXPECT_TRUE(buffer_pool_->unpin_page(all_pages[i], false).has_value());
    }
}

TEST_F(BufferPoolTest, CannotEvictPinnedPages) {
    // Fill pool with pinned pages
    std::vector<PageId> pinned_pages;
    
    for (int i = 0; i < 10; ++i) {
        auto result = buffer_pool_->new_page();
        ASSERT_TRUE(result.has_value());
        pinned_pages.push_back((*result)->id());
        // Don't unpin!
    }
    
    // Try to create another page - should fail
    auto result = buffer_pool_->new_page();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::NoAvailableFrames);
    
    // Unpin all
    for (PageId id : pinned_pages) {
        EXPECT_TRUE(buffer_pool_->unpin_page(id, false).has_value());
    }
    
    // Now should succeed
    result = buffer_pool_->new_page();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(buffer_pool_->unpin_page((*result)->id(), false).has_value());
}

// ==============================================================================
// Delete Page
// ==============================================================================

TEST_F(BufferPoolTest, DeletePage) {
    auto result = buffer_pool_->new_page();
    ASSERT_TRUE(result.has_value());
    
    PageId page_id = (*result)->id();
    EXPECT_TRUE(buffer_pool_->unpin_page(page_id, false).has_value());
    
    EXPECT_TRUE(buffer_pool_->delete_page(page_id).has_value());
}

TEST_F(BufferPoolTest, CannotDeletePinnedPage) {
    auto result = buffer_pool_->new_page();
    ASSERT_TRUE(result.has_value());
    
    PageId page_id = (*result)->id();
    // Don't unpin
    
    auto delete_result = buffer_pool_->delete_page(page_id);
    EXPECT_FALSE(delete_result.has_value());
    EXPECT_EQ(delete_result.error().code(), ErrorCode::PagePinned);
    
    EXPECT_TRUE(buffer_pool_->unpin_page(page_id, false).has_value());
    EXPECT_TRUE(buffer_pool_->delete_page(page_id).has_value());
}

// ==============================================================================
// Statistics
// ==============================================================================

TEST_F(BufferPoolTest, Statistics) {
    EXPECT_EQ(buffer_pool_->capacity(), 10);
    EXPECT_EQ(buffer_pool_->size(), 0);
    EXPECT_EQ(buffer_pool_->dirty_count(), 0);
    EXPECT_EQ(buffer_pool_->pinned_count(), 0);
    
    auto result = buffer_pool_->new_page();
    ASSERT_TRUE(result.has_value());
    
    EXPECT_EQ(buffer_pool_->size(), 1);
    EXPECT_EQ(buffer_pool_->pinned_count(), 1);
    
    EXPECT_TRUE(buffer_pool_->unpin_page((*result)->id(), true).has_value());
    
    EXPECT_EQ(buffer_pool_->pinned_count(), 0);
    EXPECT_EQ(buffer_pool_->dirty_count(), 1);
}
