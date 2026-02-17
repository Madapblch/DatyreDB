// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - WAL Unit Tests                                                   ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include <gtest/gtest.h>

#include "internal/storage/wal.hpp"

#include <filesystem>

using namespace datyredb::storage;

class WALTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "datyredb_wal_test";
        std::filesystem::remove_all(test_dir_);
        std::filesystem::create_directories(test_dir_);
        
        metrics_ = std::make_shared<CheckpointMetrics>();
        wal_ = std::make_shared<WriteAheadLog>(test_dir_, 1024 * 1024, metrics_);
        ASSERT_TRUE(wal_->open().has_value());
    }
    
    void TearDown() override {
        wal_->close();
        wal_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    std::filesystem::path test_dir_;
    std::shared_ptr<CheckpointMetrics> metrics_;
    std::shared_ptr<WriteAheadLog> wal_;
};

// ==============================================================================
// Basic Operations
// ==============================================================================

TEST_F(WALTest, AppendRecord) {
    LogRecord record(LogRecordType::Insert, 1);
    record.header.page_id = 42;
    record.data = {'H', 'e', 'l', 'l', 'o'};
    
    auto result = wal_->append(record);
    ASSERT_TRUE(result.has_value());
    
    Lsn lsn = *result;
    EXPECT_NE(lsn, INVALID_LSN);
    EXPECT_GT(lsn, 0);
}

TEST_F(WALTest, LSNIncrement) {
    LogRecord record(LogRecordType::Insert);
    
    auto result1 = wal_->append(record);
    auto result2 = wal_->append(record);
    auto result3 = wal_->append(record);
    
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_TRUE(result3.has_value());
    
    EXPECT_LT(*result1, *result2);
    EXPECT_LT(*result2, *result3);
}

TEST_F(WALTest, CurrentLSN) {
    EXPECT_EQ(wal_->current_lsn(), 1);  // Starts at 1
    
    LogRecord record(LogRecordType::Insert);
    
    ASSERT_TRUE(wal_->append(record).has_value());
    EXPECT_EQ(wal_->current_lsn(), 2);
    
    ASSERT_TRUE(wal_->append(record).has_value());
    EXPECT_EQ(wal_->current_lsn(), 3);
}

// ==============================================================================
// Force (Durability)
// ==============================================================================

TEST_F(WALTest, Force) {
    LogRecord record(LogRecordType::TxnCommit, 1);
    
    auto result = wal_->append(record);
    ASSERT_TRUE(result.has_value());
    
    Lsn lsn = *result;
    
    EXPECT_TRUE(wal_->force(lsn).has_value());
    EXPECT_GE(wal_->flushed_lsn(), lsn);
}

TEST_F(WALTest, ForceAll) {
    for (int i = 0; i < 10; ++i) {
        LogRecord record(LogRecordType::Insert);
        ASSERT_TRUE(wal_->append(record).has_value());
    }
    
    EXPECT_TRUE(wal_->force_all().has_value());
    EXPECT_GE(wal_->flushed_lsn(), wal_->current_lsn() - 1);
}

// ==============================================================================
// Checkpoint Records
// ==============================================================================

TEST_F(WALTest, CheckpointRecords) {
    auto begin_result = wal_->write_checkpoint_begin();
    ASSERT_TRUE(begin_result.has_value());
    
    Lsn begin_lsn = *begin_result;
    EXPECT_NE(begin_lsn, INVALID_LSN);
    
    auto end_result = wal_->write_checkpoint_end(begin_lsn);
    ASSERT_TRUE(end_result.has_value());
    
    Lsn end_lsn = *end_result;
    EXPECT_GT(end_lsn, begin_lsn);
}

// ==============================================================================
// Transaction Records
// ==============================================================================

TEST_F(WALTest, TransactionLifecycle) {
    TxnId txn_id = 42;
    
    auto begin_result = wal_->write_txn_begin(txn_id);
    ASSERT_TRUE(begin_result.has_value());
    
    // Some operations
    LogRecord record(LogRecordType::Insert, txn_id);
    ASSERT_TRUE(wal_->append(record).has_value());
    ASSERT_TRUE(wal_->append(record).has_value());
    
    auto commit_result = wal_->write_txn_commit(txn_id);
    ASSERT_TRUE(commit_result.has_value());
    
    // Commit should be durable
    EXPECT_GE(wal_->flushed_lsn(), *commit_result);
}

TEST_F(WALTest, TransactionAbort) {
    TxnId txn_id = 99;
    
    auto begin_result = wal_->write_txn_begin(txn_id);
    ASSERT_TRUE(begin_result.has_value());
    
    auto abort_result = wal_->write_txn_abort(txn_id);
    ASSERT_TRUE(abort_result.has_value());
}

// ==============================================================================
// Size Tracking
// ==============================================================================

TEST_F(WALTest, SizeTracking) {
    std::uint64_t initial_size = wal_->size();
    
    LogRecord record(LogRecordType::Insert);
    record.data.resize(100, 'X');
    
    ASSERT_TRUE(wal_->append(record).has_value());
    
    EXPECT_GT(wal_->size(), initial_size);
}

// ==============================================================================
// Log Record Serialization
// ==============================================================================

TEST_F(WALTest, LogRecordSerialization) {
    LogRecord record(LogRecordType::Update, 99);
    record.header.page_id = 42;
    record.header.offset = 100;
    record.header.prev_lsn = 12340;
    record.data = {'T', 'e', 's', 't', 'D', 'a', 't', 'a'};
    
    std::vector<char> buffer;
    record.serialize(buffer);
    
    auto result = LogRecord::deserialize(buffer);
    ASSERT_TRUE(result.has_value());
    
    LogRecord& deserialized = *result;
    
    EXPECT_EQ(deserialized.header.type, LogRecordType::Update);
    EXPECT_EQ(deserialized.header.txn_id, 99);
    EXPECT_EQ(deserialized.header.page_id, 42);
    EXPECT_EQ(deserialized.header.offset, 100);
    EXPECT_EQ(deserialized.header.prev_lsn, 12340);
    EXPECT_EQ(deserialized.data, record.data);
}

TEST_F(WALTest, LogRecordChecksum) {
    LogRecord record(LogRecordType::Insert);
    record.data = {'D', 'a', 't', 'a'};
    
    std::vector<char> buffer;
    record.serialize(buffer);
    
    // Verify checksum is valid
    auto result = LogRecord::deserialize(buffer);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->verify_checksum());
    
    // Corrupt data (using static_cast to avoid warnings)
    buffer[buffer.size() - 1] ^= static_cast<char>(0xFF);
    
    auto corrupt_result = LogRecord::deserialize(buffer);
    EXPECT_FALSE(corrupt_result.has_value());
}
