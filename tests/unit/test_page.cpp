// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Page Unit Tests                                                  ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include <gtest/gtest.h>

#include "internal/storage/page.hpp"

using namespace datyredb::storage;

class PageTest : public ::testing::Test {
protected:
    Page page;
};

// ==============================================================================
// Construction Tests
// ==============================================================================

TEST_F(PageTest, DefaultConstruction) {
    EXPECT_EQ(page.id(), INVALID_PAGE_ID);
    EXPECT_FALSE(page.is_dirty());
    EXPECT_FALSE(page.is_pinned());
    EXPECT_EQ(page.pin_count(), 0);
}

TEST_F(PageTest, ConstructionWithId) {
    Page p(42);
    EXPECT_EQ(p.id(), 42);
    EXPECT_FALSE(p.is_dirty());
    EXPECT_FALSE(p.is_pinned());
}

// ==============================================================================
// Dirty Flag Tests
// ==============================================================================

TEST_F(PageTest, DirtyFlag) {
    EXPECT_FALSE(page.is_dirty());
    
    page.mark_dirty();
    EXPECT_TRUE(page.is_dirty());
    
    page.mark_clean();
    EXPECT_FALSE(page.is_dirty());
}

// ==============================================================================
// Pin Count Tests
// ==============================================================================

TEST_F(PageTest, PinCount) {
    EXPECT_EQ(page.pin_count(), 0);
    EXPECT_FALSE(page.is_pinned());
    
    page.pin();
    EXPECT_EQ(page.pin_count(), 1);
    EXPECT_TRUE(page.is_pinned());
    
    page.pin();
    EXPECT_EQ(page.pin_count(), 2);
    
    page.unpin();
    EXPECT_EQ(page.pin_count(), 1);
    EXPECT_TRUE(page.is_pinned());
    
    page.unpin();
    EXPECT_EQ(page.pin_count(), 0);
    EXPECT_FALSE(page.is_pinned());
}

TEST_F(PageTest, PinCountDoesNotGoNegative) {
    EXPECT_EQ(page.pin_count(), 0);
    
    page.unpin();
    EXPECT_EQ(page.pin_count(), 0);
    
    page.unpin();
    EXPECT_EQ(page.pin_count(), 0);
}

// ==============================================================================
// LSN Tests
// ==============================================================================

TEST_F(PageTest, LSN) {
    Page p(1);
    EXPECT_EQ(p.lsn(), 0);
    
    p.set_lsn(12345);
    EXPECT_EQ(p.lsn(), 12345);
    
    p.set_lsn(INVALID_LSN);
    EXPECT_EQ(p.lsn(), INVALID_LSN);
}

// ==============================================================================
// Flags Tests
// ==============================================================================

TEST_F(PageTest, Flags) {
    Page p(1);
    
    EXPECT_FALSE(p.has_flags(PageFlags::Leaf));
    EXPECT_FALSE(p.has_flags(PageFlags::Internal));
    
    p.add_flags(PageFlags::Leaf);
    EXPECT_TRUE(p.has_flags(PageFlags::Leaf));
    EXPECT_FALSE(p.has_flags(PageFlags::Internal));
    
    p.add_flags(PageFlags::Root);
    EXPECT_TRUE(p.has_flags(PageFlags::Leaf));
    EXPECT_TRUE(p.has_flags(PageFlags::Root));
    
    p.remove_flags(PageFlags::Leaf);
    EXPECT_FALSE(p.has_flags(PageFlags::Leaf));
    EXPECT_TRUE(p.has_flags(PageFlags::Root));
}

// ==============================================================================
// Checksum Tests
// ==============================================================================

TEST_F(PageTest, ChecksumInitiallyValid) {
    Page p(1);
    p.update_checksum();
    EXPECT_TRUE(p.verify_checksum());
}

TEST_F(PageTest, ChecksumDetectsCorruption) {
    Page p(1);
    
    // Write some data
    const char* test_data = "Hello, DatyreDB!";
    std::memcpy(p.payload(), test_data, std::strlen(test_data));
    
    p.update_checksum();
    EXPECT_TRUE(p.verify_checksum());
    
    // Corrupt data
    p.payload()[0] = 'X';
    EXPECT_FALSE(p.verify_checksum());
    
    // Fix checksum
    p.update_checksum();
    EXPECT_TRUE(p.verify_checksum());
}

// ==============================================================================
// Data Access Tests
// ==============================================================================

TEST_F(PageTest, PayloadSize) {
    EXPECT_EQ(Page::payload_size(), PAGE_SIZE - PageHeader::SIZE);
    EXPECT_GT(Page::payload_size(), 4000);  // Should have plenty of space
}

TEST_F(PageTest, DataAccess) {
    Page p(1);
    
    // Write to payload
    std::memset(p.payload(), 0xAB, 100);
    
    // Read back
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(static_cast<unsigned char>(p.payload()[i]), 0xAB);
    }
}

TEST_F(PageTest, FreeSpace) {
    Page p(1);
    
    EXPECT_EQ(p.free_space(), Page::payload_size());
    
    p.set_free_space(1000);
    EXPECT_EQ(p.free_space(), 1000);
}

// ==============================================================================
// Reset Tests
// ==============================================================================

TEST_F(PageTest, Reset) {
    Page p(42);
    p.mark_dirty();
    p.pin();
    p.pin();
    p.set_lsn(100);
    p.add_flags(PageFlags::Leaf);
    
    // Write data
    std::memset(p.payload(), 0xFF, 100);
    
    p.reset();
    
    EXPECT_EQ(p.id(), INVALID_PAGE_ID);
    EXPECT_FALSE(p.is_dirty());
    EXPECT_EQ(p.pin_count(), 0);
    EXPECT_FALSE(p.is_pinned());
    EXPECT_EQ(p.free_space(), Page::payload_size());
}

TEST_F(PageTest, ResetWithNewId) {
    Page p(42);
    p.mark_dirty();
    p.pin();
    
    p.reset(99);
    
    EXPECT_EQ(p.id(), 99);
    EXPECT_FALSE(p.is_dirty());
    EXPECT_EQ(p.pin_count(), 0);
}

// ==============================================================================
// Move Tests
// ==============================================================================

TEST_F(PageTest, MoveConstruction) {
    Page p1(42);
    p1.mark_dirty();
    p1.pin();
    std::memset(p1.payload(), 0xAB, 100);
    
    Page p2(std::move(p1));
    
    EXPECT_EQ(p2.id(), 42);
    EXPECT_TRUE(p2.is_dirty());
    EXPECT_EQ(p2.pin_count(), 1);
    EXPECT_EQ(static_cast<unsigned char>(p2.payload()[0]), 0xAB);
    
    // p1 should be reset
    EXPECT_EQ(p1.id(), INVALID_PAGE_ID);
    EXPECT_FALSE(p1.is_dirty());
    EXPECT_EQ(p1.pin_count(), 0);
}

TEST_F(PageTest, MoveAssignment) {
    Page p1(42);
    p1.mark_dirty();
    
    Page p2(99);
    p2 = std::move(p1);
    
    EXPECT_EQ(p2.id(), 42);
    EXPECT_TRUE(p2.is_dirty());
    
    EXPECT_EQ(p1.id(), INVALID_PAGE_ID);
}
