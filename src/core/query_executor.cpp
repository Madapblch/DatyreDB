#include "core/query_executor.hpp"
#include "core/storage_engine.hpp"
#include "utils/logger.hpp"

#include <algorithm>
#include <sstream>
#include <regex>
#include <cctype>

namespace datyredb {

QueryExecutor::QueryExecutor(StorageEngine* storage)
    : storage_(storage) {
    Logger::debug("QueryExecutor created");
}

QueryExecutor::~QueryExecutor() {
    shutdown();
}

bool QueryExecutor::initialize() {
    Logger::info("Query executor initialized");
    return true;
}

void QueryExecutor::shutdown() {
    Logger::info("Query executor shutdown");
}

QueryResult QueryExecutor::execute(const std::string& sql) {
    Logger::debug("Executing query: {}", sql);
    
    std::string upper_sql = to_upper(trim(sql));
    
    if (upper_sql.find("SELECT") == 0) {
        return execute_select(sql);
    } else if (upper_sql.find("INSERT") == 0) {
        return execute_insert(sql);
    } else if (upper_sql.find("CREATE TABLE") == 0) {
        return execute_create_table(sql);
    } else if (upper_sql.find("SHOW TABLES") == 0) {
        return execute_show_tables();
    }
    
    QueryResult result;
    result.success = false;
    result.error_message = "Unsupported SQL command";
    return result;
}

QueryResult QueryExecutor::execute_select(const std::string& sql) {
    QueryResult result;
    
    // Parse: SELECT * FROM table_name
    std::regex select_regex(R"(SELECT\s+\*\s+FROM\s+(\w+))", std::regex::icase);
    std::smatch match;
    
    if (!std::regex_search(sql, match, select_regex)) {
        result.success = false;
        result.error_message = "Invalid SELECT syntax. Use: SELECT * FROM table_name";
        return result;
    }
    
    std::string table_name = match[1].str();
    
    // Get columns
    auto columns = storage_->get_table_columns(table_name);
    if (columns.empty()) {
        result.success = false;
        result.error_message = "Table '" + table_name + "' not found";
        return result;
    }
    
    result.columns = columns;
    result.rows = storage_->select(table_name);
    result.success = true;
    
    return result;
}

QueryResult QueryExecutor::execute_insert(const std::string& sql) {
    QueryResult result;
    
    // Parse: INSERT INTO table VALUES (val1, val2, ...)
    std::regex insert_regex(R"(INSERT\s+INTO\s+(\w+)\s+VALUES\s*\((.*)\))", std::regex::icase);
    std::smatch match;
    
    if (!std::regex_search(sql, match, insert_regex)) {
        result.success = false;
        result.error_message = "Invalid INSERT syntax. Use: INSERT INTO table VALUES (val1, val2, ...)";
        return result;
    }
    
    std::string table_name = match[1].str();
    std::string values_str = match[2].str();
    
    // Parse values
    std::vector<std::string> values;
    std::stringstream ss(values_str);
    std::string value;
    
    while (std::getline(ss, value, ',')) {
        value = trim(value);
        
        // Remove quotes
        if (!value.empty()) {
            if ((value.front() == '\'' && value.back() == '\'') ||
                (value.front() == '"' && value.back() == '"')) {
                value = value.substr(1, value.size() - 2);
            }
        }
        
        values.push_back(value);
    }
    
    if (storage_->insert(table_name, values)) {
        result.success = true;
        result.affected_rows = 1;
    } else {
        result.success = false;
        result.error_message = "Failed to insert data. Table may not exist.";
    }
    
    return result;
}

QueryResult QueryExecutor::execute_create_table(const std::string& sql) {
    QueryResult result;
    
    // Parse: CREATE TABLE name (col1 TYPE, col2 TYPE, ...)
    std::regex create_regex(R"(CREATE\s+TABLE\s+(\w+)\s*\((.*)\))", std::regex::icase);
    std::smatch match;
    
    if (!std::regex_search(sql, match, create_regex)) {
        result.success = false;
        result.error_message = "Invalid CREATE TABLE syntax";
        return result;
    }
    
    std::string table_name = match[1].str();
    std::string columns_str = match[2].str();
    
    // Parse columns
    std::vector<std::string> columns;
    std::stringstream ss(columns_str);
    std::string column;
    
    while (std::getline(ss, column, ',')) {
        column = trim(column);
        
        // Extract column name (first word)
        size_t space_pos = column.find(' ');
        if (space_pos != std::string::npos) {
            column = column.substr(0, space_pos);
        }
        
        if (!column.empty()) {
            columns.push_back(column);
        }
    }
    
    if (storage_->create_table(table_name, columns)) {
        result.success = true;
        result.affected_rows = 0;
    } else {
        result.success = false;
        result.error_message = "Table '" + table_name + "' already exists";
    }
    
    return result;
}

QueryResult QueryExecutor::execute_show_tables() {
    QueryResult result;
    
    result.columns = {"Tables"};
    auto tables = storage_->list_tables();
    
    for (const auto& table : tables) {
        result.rows.push_back({table});
    }
    
    result.success = true;
    return result;
}

std::string QueryExecutor::to_upper(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return upper;
}

std::string QueryExecutor::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

} // namespace datyredb
