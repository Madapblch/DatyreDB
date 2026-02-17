#pragma once

#include "core/database.hpp"
#include <string>
#include <memory>

namespace datyredb {

class StorageEngine;

class QueryExecutor {
public:
    explicit QueryExecutor(StorageEngine* storage);
    ~QueryExecutor();
    
    bool initialize();
    void shutdown();
    
    QueryResult execute(const std::string& sql);
    
private:
    StorageEngine* storage_;
    
    QueryResult execute_select(const std::string& sql);
    QueryResult execute_insert(const std::string& sql);
    QueryResult execute_create_table(const std::string& sql);
    QueryResult execute_show_tables();
    
    static std::string to_upper(const std::string& str);
    static std::string trim(const std::string& str);
};

} // namespace datyredb
