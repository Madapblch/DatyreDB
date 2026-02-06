#pragma once

#include "../storage/disk_manager.h"
#include "../buffer/buffer_pool.h"
#include "../common/types.h"
#include "../storage/wal.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

class Database {
public:
    // enable_shutdown_checkpoint=true: like a real DBMS, do a clean checkpoint on shutdown.
    // Tests can disable it to simulate a crash (leaving WAL intact).
    explicit Database(bool enable_shutdown_checkpoint = true);
    ~Database();

    Status Execute(const std::string& command);

private:
    std::mutex db_mutex_; // Thread-safe access for concurrent clients
    std::unordered_map<std::string, std::unique_ptr<DiskManager>> disk_managers_;
    std::unordered_map<std::string, std::unique_ptr<BufferPool>> buffer_pools_;
    std::unordered_map<std::string, std::unique_ptr<WAL>> wals_;
    std::unordered_map<std::string, std::unordered_map<std::string, Table>> tables_; // db_name -> table_name -> Table
    
    // Simple indexes: db_name -> table_name -> column_name -> map(value -> row_ids)
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::vector<size_t>>>>> indexes_;
    
    std::string current_db_;

    // Transaction state
    bool in_transaction_{false};
    bool autocommit_{true}; // Default autocommit like in prof DBMS
    std::unordered_map<page_id_t, Page> pending_changes_; // page_id -> modified page

    bool enable_shutdown_checkpoint_{true};

    // Helper methods for WHERE clause and filtering
    bool EvaluateCondition(const Row& row, const Table& table, const WhereCondition& condition) const;
    bool MatchesConditions(const Row& row, const Table& table, const std::vector<WhereCondition>& conditions) const;
    
    // Helper to parse WHERE conditions from query string
    std::vector<WhereCondition> ParseWhereClause(const std::string& where_str, const Table& table) const;
    
    // Helper to parse comparison operator
    ComparisonOp ParseOperator(const std::string& op_str) const;

    // Catalog: persist/load table metadata on page 1
    static constexpr page_id_t kCatalogPageId = 1;
    Status SaveCatalog(const std::string& db_name);
    Status LoadCatalog(const std::string& db_name);

    friend class Server; // Allow Server to lock mutex
};