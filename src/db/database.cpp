#include "database.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring> // std::strlen
#include <cstdio>  // std::snprintf, std::sscanf
#include <algorithm> // std::find_if

Database::Database(bool enable_shutdown_checkpoint)
    : enable_shutdown_checkpoint_(enable_shutdown_checkpoint) {
    // Don't initialize any database by default
    // Wait for CREATE DATABASE or USE
    current_db_ = "";
}

Database::~Database() {
    if (!enable_shutdown_checkpoint_) {
        return; // simulate "crash-like" shutdown for tests/tools
    }
    // On clean shutdown, try to checkpoint (truncate WAL) for all databases
    for (auto& pair : wals_) {
        const std::string& db_name = pair.first;
        if (pair.second) {
            (void)pair.second->Flush();
        }
        // Ensure DB file is durable before truncating WAL (checkpoint-like shutdown).
        if (disk_managers_.count(db_name) && disk_managers_[db_name]) {
            (void)disk_managers_[db_name]->Sync();
        }
        if (pair.second) {
            (void)pair.second->Truncate();
        }
    }
}

Status Database::Execute(const std::string& command) {
    std::stringstream ss(command);
    std::string op;
    ss >> op;
    // Remove trailing ;
    if (!op.empty() && op.back() == ';') op.pop_back();

    if (op == "SET") {
        std::string sub;
        ss >> sub;
        if (sub == "AUTOCOMMIT") {
            std::string on_off;
            ss >> on_off;
            if (!on_off.empty() && on_off.back() == ';') on_off.pop_back();
            if (on_off == "ON") {
                autocommit_ = true;
                std::cout << "Autocommit enabled\n";
            } else if (on_off == "OFF") {
                autocommit_ = false;
                std::cout << "Autocommit disabled\n";
            } else {
                std::cout << "ERROR: Syntax: SET AUTOCOMMIT ON/OFF;\n";
                return Status::ERROR;
            }
            return Status::OK;
        } else {
            // This is SET key value
            std::string key = sub;
            std::string value;
            std::getline(ss, value);
            if (!value.empty() && value[0] == ' ') value.erase(0, 1);
            if (!value.empty() && value.back() == ';') value.pop_back();

            if (current_db_.empty()) {
                std::cout << "ERROR: No database selected. CREATE DATABASE name; USE name;\n";
                return Status::ERROR;
            }

            Page* page = buffer_pools_[current_db_]->GetPage(0);
            if (!page) {
                std::cerr << "Failed to get page 0 from buffer pool\n";
                return Status::ERROR;
            }

            // If in transaction, work with pending changes
            Page current_page = in_transaction_ && pending_changes_.count(0) ? pending_changes_[0] : *page;

            // Подготавливаем новую версию страницы
            Page new_page = current_page;
            std::snprintf(new_page.data(), PAGE_SIZE, "%s %s",
                          key.c_str(), value.c_str());

            if (in_transaction_) {
                // Store in pending changes
                pending_changes_[0] = new_page;
            } else {
                if (autocommit_) {
                    // Autocommit: wrap in transaction
                    in_transaction_ = true;
                    pending_changes_[0] = new_page;
                    // Simulate commit
                    if (wals_[current_db_]->AppendPageRecord(0, new_page) != Status::OK ||
                        wals_[current_db_]->Flush() != Status::OK) {
                        std::cerr << "WAL write/flush failed\n";
                        in_transaction_ = false;
                        pending_changes_.clear();
                        return Status::ERROR;
                    }
                    Status s = disk_managers_[current_db_]->WritePage(0, new_page);
                    if (s != Status::OK) {
                        std::cerr << "WritePage failed\n";
                        in_transaction_ = false;
                        pending_changes_.clear();
                        return Status::ERROR;
                    }
                    buffer_pools_[current_db_]->Invalidate(0);
                    in_transaction_ = false;
                    pending_changes_.clear();
                } else {
                    std::cout << "ERROR: Not in transaction. Use BEGIN; first or SET AUTOCOMMIT ON;\n";
                    return Status::ERROR;
                }
            }
            std::cout << "OK\n";
            return Status::OK;
        }

    } else if (op == "GET") {
        std::string key;
        ss >> key;

        // Убрать ; если есть
        if (!key.empty() && key.back() == ';') key.pop_back();

        if (current_db_.empty()) {
            std::cout << "ERROR: No database selected. CREATE DATABASE name; USE name;\n";
            return Status::ERROR;
        }

        Page* page = buffer_pools_[current_db_]->GetPage(0);
        if (!page) {
            std::cout << "(nil)\n";
            return Status::OK;
        }

        // If in transaction, check pending changes first
        const Page& source_page = in_transaction_ && pending_changes_.count(0) ? pending_changes_[0] : *page;

        char stored_key[128] = {0};
        std::sscanf(source_page.data(), "%127s", stored_key);

        if (key == stored_key) {
            const char* value_start = source_page.data() + std::strlen(stored_key);
            if (*value_start == ' ')
                ++value_start;
            std::cout << value_start << "\n";
        } else {
            std::cout << "(nil)\n";
        }
        return Status::OK;
    } else if (op == "CREATE") {
        std::string create_what;
        ss >> create_what;
        if (create_what == "DATABASE") {
            std::string db_name;
            ss >> db_name;
            if (!db_name.empty() && db_name.back() == ';') db_name.pop_back();
            if (db_name.empty()) {
                std::cout << "ERROR: Syntax: CREATE DATABASE db_name;\n";
                return Status::ERROR;
            }
            if (disk_managers_.find(db_name) != disk_managers_.end()) {
                std::cout << "ERROR: Database already exists\n";
                return Status::ERROR;
            }
            disk_managers_[db_name] = std::make_unique<DiskManager>(db_name);
            if (disk_managers_[db_name]->GetTotalPages() == 0) {
                disk_managers_[db_name]->AllocatePage();
                disk_managers_[db_name]->AllocatePage();
                Page catalog_page{};
                std::snprintf(catalog_page.data(), PAGE_SIZE, "CATALOG\n");
                (void)disk_managers_[db_name]->WritePage(kCatalogPageId, catalog_page);
                tables_[db_name] = {};
            } else {
                tables_[db_name] = {};
                if (disk_managers_[db_name]->GetTotalPages() >= 2) {
                    LoadCatalog(db_name);
                }
            }
            buffer_pools_[db_name] = std::make_unique<BufferPool>(disk_managers_[db_name].get(), 128);
            wals_[db_name] = std::make_unique<WAL>(db_name + ".wal");
            wals_[db_name]->Recover([this, &db_name](page_id_t pid, const Page& page) {
                (void)disk_managers_[db_name]->WritePage(pid, page);
                buffer_pools_[db_name]->Invalidate(pid);
            });
            std::cout << "Database created\n";
            return Status::OK;
        }
        if (create_what == "TABLE") {
            if (current_db_.empty()) {
std::cout << "ERROR: No database selected. USE db_name;\n";
                return Status::ERROR;
        }
        std::string table_name;
        ss >> table_name;
        std::string rest;
        std::getline(ss, rest);
        if (rest.find('(') == std::string::npos || rest.find(')') == std::string::npos) {
            std::cout << "ERROR: Syntax: CREATE TABLE name (col type, ...);\n";
                return Status::ERROR;
            }
            size_t start = rest.find('(') + 1;
            size_t end = rest.find(')');
            std::string cols_str = rest.substr(start, end - start);
            std::stringstream cols_ss(cols_str);
            std::string col_def;
            Table table;
            table.name = table_name;
            while (std::getline(cols_ss, col_def, ',')) {
                std::stringstream col_ss(col_def);
                std::string col_name, type_str;
                col_ss >> col_name >> type_str;
                DataType type = (type_str == "INT") ? DataType::INT : DataType::VARCHAR;
                table.columns.push_back({col_name, type});
            }
            table.first_page = disk_managers_[current_db_]->AllocatePage();
            tables_[current_db_][table_name] = std::move(table);
            if (SaveCatalog(current_db_) != Status::OK) {
                std::cout << "Warning: catalog save failed\n";
            }
            std::cout << "Table created\n";
            return Status::OK;
        }
        std::cout << "ERROR: Syntax: CREATE DATABASE name; or CREATE TABLE name (col type, ...);\n";
        return Status::ERROR;
    } else if (op == "USE") {
        std::string db_name;
        ss >> db_name;
        // Убрать ; если есть
        if (!db_name.empty() && db_name.back() == ';') db_name.pop_back();

        if (disk_managers_.find(db_name) == disk_managers_.end()) {
            std::cout << "ERROR: Database does not exist\n";
            return Status::ERROR;
        }

        // Recover WAL on USE to bring DB file to latest durable state.
        if (wals_.find(db_name) != wals_.end() && wals_[db_name]) {
            wals_[db_name]->Recover([this, &db_name](page_id_t pid, const Page& page) {
                (void)disk_managers_[db_name]->WritePage(pid, page);
                buffer_pools_[db_name]->Invalidate(pid);
            });
        }

        // Load table catalog from disk so CREATE TABLE / INSERT etc. see existing tables
        if (LoadCatalog(db_name) != Status::OK) {
            std::cout << "Warning: could not load catalog for " << db_name << "\n";
        }
        current_db_ = db_name;
        std::cout << "Switched to database " << db_name << "\n";
        return Status::OK;
    } else if (op == "BEGIN") {
        if (in_transaction_) {
            std::cout << "ERROR: Transaction already in progress\n";
            return Status::ERROR;
        }
        in_transaction_ = true;
        pending_changes_.clear();
        std::cout << "Transaction started\n";
        return Status::OK;
    } else if (op == "COMMIT") {
        if (!in_transaction_) {
            std::cout << "ERROR: No transaction in progress\n";
            return Status::ERROR;
        }
        // Apply all pending changes
        for (const auto& change : pending_changes_) {
            page_id_t pid = change.first;
            const Page& page = change.second;
            if (wals_[current_db_]->AppendPageRecord(pid, page) != Status::OK ||
                wals_[current_db_]->Flush() != Status::OK) {
                std::cerr << "WAL write/flush failed during commit\n";
                return Status::ERROR;
            }
            if (disk_managers_[current_db_]->WritePage(pid, page) != Status::OK) {
                std::cerr << "WritePage failed during commit\n";
                return Status::ERROR;
            }
            // Update buffer pool
            buffer_pools_[current_db_]->Invalidate(pid); // Force reload
        }
        pending_changes_.clear();
        in_transaction_ = false;
        std::cout << "Transaction committed\n";
        return Status::OK;
    } else if (op == "ROLLBACK") {
        if (!in_transaction_) {
            std::cout << "ERROR: No transaction in progress\n";
            return Status::ERROR;
        }
        pending_changes_.clear();
        in_transaction_ = false;
        std::cout << "Transaction rolled back\n";
        return Status::OK;
    } else if (op == "CHECKPOINT") {
        if (current_db_.empty()) {
            std::cout << "No database selected. Use USE name;\n";
            return Status::ERROR;
        }
        // Minimal checkpoint: make WAL durable, make DB file durable, then truncate WAL.
        uint64_t checkpoint_lsn = 0;
        if (wals_.count(current_db_) && wals_[current_db_]) {
            if (wals_[current_db_]->Flush() != Status::OK) {
                std::cout << "Checkpoint failed: WAL flush error\n";
                return Status::ERROR;
            }
            checkpoint_lsn = wals_[current_db_]->NextLSN();
        }
        if (disk_managers_.count(current_db_) && disk_managers_[current_db_]) {
            if (disk_managers_[current_db_]->Sync() != Status::OK) {
                std::cout << "Checkpoint failed: DB sync error\n";
                return Status::ERROR;
            }
        }
        if (wals_.count(current_db_) && wals_[current_db_]) {
            // For now we truncate the whole WAL (checkpoint_lsn is end-of-log),
            // but the API allows truncating only up to a stable LSN.
            if (wals_[current_db_]->TruncateUpTo(checkpoint_lsn) != Status::OK) {
                std::cout << "Checkpoint failed: WAL truncate error\n";
                return Status::ERROR;
            }
        }
        std::cout << "Checkpoint completed\n";
        return Status::OK;
    } else if (op == "EXIT") {
        return Status::OK;
    } else if (op == "SHOW") {
        std::string what;
        ss >> what;
        if (!what.empty() && what.back() == ';') what.pop_back();
        if (what == "DATABASES") {
            if (disk_managers_.empty()) {
                std::cout << "(no databases)\n";
            } else {
                for (const auto& kv : disk_managers_) {
                    std::cout << kv.first << "\n";
                }
            }
            return Status::OK;
        }
        if (what == "TABLES") {
            if (current_db_.empty()) {
                std::cout << "ERROR: No database selected. USE db_name;\n";
                return Status::ERROR;
            }
            const auto& tbl = tables_[current_db_];
            if (tbl.empty()) {
                std::cout << "(no tables)\n";
            } else {
                for (const auto& kv : tbl) {
                    std::cout << kv.first << "\n";
                }
            }
            return Status::OK;
        }
        std::cout << "ERROR: Syntax: SHOW DATABASES; or SHOW TABLES;\n";
        return Status::ERROR;
    } else if (op == "DESCRIBE") {
        std::string table_name;
        ss >> table_name;
        if (!table_name.empty() && table_name.back() == ';') table_name.pop_back();
        if (current_db_.empty()) {
            std::cout << "ERROR: No database selected. USE db_name;\n";
            return Status::ERROR;
        }
        auto it = tables_[current_db_].find(table_name);
        if (it == tables_[current_db_].end()) {
            std::cout << "ERROR: Table does not exist\n";
            return Status::ERROR;
        }
        const Table& t = it->second;
        std::cout << "Column\tType\n";
        for (const auto& col : t.columns) {
            std::cout << col.name << "\t" << (col.type == DataType::INT ? "INT" : "VARCHAR") << "\n";
        }
        return Status::OK;
    } else if (op == "DROP") {
        std::string what, name;
        ss >> what >> name;
        if (!name.empty() && name.back() == ';') name.pop_back();
        if (what == "DATABASE") {
            if (disk_managers_.find(name) == disk_managers_.end()) {
                std::cout << "Database does not exist\n";
                return Status::ERROR;
            }
            if (name == current_db_) {
                std::cout << "Cannot drop current database\n";
                return Status::ERROR;
            }
            disk_managers_.erase(name);
            buffer_pools_.erase(name);
            wals_.erase(name);
            tables_.erase(name);
            indexes_.erase(name);
            std::cout << "Database dropped\n";
            return Status::OK;
        }
        if (what == "TABLE") {
            if (current_db_.empty()) {
                std::cout << "ERROR: No database selected\n";
                return Status::ERROR;
            }
            auto it = tables_[current_db_].find(name);
            if (it == tables_[current_db_].end()) {
                std::cout << "ERROR: Table does not exist\n";
                return Status::ERROR;
            }
            tables_[current_db_].erase(it);
            if (indexes_.count(current_db_)) {
                auto& db_idx = indexes_[current_db_];
                if (db_idx.count(name)) db_idx.erase(name);
            }
            if (SaveCatalog(current_db_) != Status::OK) {
                std::cout << "Warning: catalog save failed\n";
            }
            std::cout << "Table dropped\n";
            return Status::OK;
        }
        std::cout << "ERROR: Syntax: DROP DATABASE name; or DROP TABLE name;\n";
        return Status::ERROR;
    } else if (op == "INSERT") {
        std::string into_keyword;
        ss >> into_keyword;
        if (into_keyword == "INTO") {
            if (current_db_.empty()) {
                std::cout << "ERROR: No database selected\n";
                return Status::ERROR;
            }
            std::string table_name;
            ss >> table_name;
            if (!table_name.empty() && table_name.back() == ';') table_name.pop_back();
            if (tables_[current_db_].find(table_name) == tables_[current_db_].end()) {
                std::cout << "ERROR: Table does not exist\n";
                return Status::ERROR;
            }
            std::string values_keyword;
            ss >> values_keyword;
            if (values_keyword != "VALUES") {
                std::cout << "ERROR: Syntax: INSERT INTO table VALUES (...);\n";
                return Status::ERROR;
            }
            std::string rest;
            std::getline(ss, rest);
            size_t start = rest.find('(') + 1;
            size_t end = rest.find(')');
            if (start == std::string::npos || end == std::string::npos || start >= end) {
                std::cout << "ERROR: Syntax: INSERT INTO table VALUES (val1, val2, ...);\n";
                return Status::ERROR;
            }
            std::string vals_str = rest.substr(start, end - start);
            std::stringstream vals_ss(vals_str);
            std::string val_str;
            Row row;
            auto& table = tables_[current_db_][table_name];
            while (std::getline(vals_ss, val_str, ',')) {
                val_str.erase(val_str.begin(), std::find_if(val_str.begin(), val_str.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                val_str.erase(std::find_if(val_str.rbegin(), val_str.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), val_str.end());
                if (val_str.empty()) continue;
                if (val_str.size() >= 2 && val_str.front() == '\'' && val_str.back() == '\'') {
                    row.values.push_back(val_str.substr(1, val_str.size() - 2));
                } else {
                    try {
                        row.values.push_back(std::stoi(val_str));
                    } catch (...) {
                        std::cout << "ERROR: Invalid value: " << val_str << "\n";
                        return Status::ERROR;
                    }
                }
            }
            if (row.values.size() != table.columns.size()) {
                std::cout << "ERROR: Column count mismatch: expected " << table.columns.size() << ", got " << row.values.size() << "\n";
                return Status::ERROR;
            }
            // Store row on page (simple: append to first page)
            Page* page = buffer_pools_[current_db_]->GetPage(table.first_page);
            if (!page) {
                std::cout << "ERROR: Failed to get page\n";
                return Status::ERROR;
            }

            // If inside an explicit transaction, append into pending version of the page.
            // Otherwise, behave like a real DBMS autocommit: wrap in an implicit single-page transaction.
            Page base_page = (in_transaction_ && pending_changes_.count(table.first_page))
                                 ? pending_changes_[table.first_page]
                                 : *page;
            Page new_page = base_page;

            // Simple storage: serialize row at end of page
            char* data = new_page.data();
            size_t offset = std::strlen(data); // Assume null-terminated
            for (size_t i = 0; i < row.values.size(); ++i) {
                if (std::holds_alternative<int>(row.values[i])) {
                    std::snprintf(data + offset, PAGE_SIZE - offset, "%d ", std::get<int>(row.values[i]));
                } else {
                    std::snprintf(data + offset, PAGE_SIZE - offset, "'%s' ", std::get<std::string>(row.values[i]).c_str());
                }
                offset += std::strlen(data + offset);
            }
            std::snprintf(data + offset, PAGE_SIZE - offset, "\n");

            if (in_transaction_) {
                // Defer WAL + disk write to COMMIT; just remember the new page image.
                pending_changes_[table.first_page] = new_page;
            } else {
                if (!autocommit_) {
                    std::cout << "ERROR: Not in transaction. Use BEGIN; first or SET AUTOCOMMIT ON;\n";
                    return Status::ERROR;
                }
                // Autocommit single-page transaction
                in_transaction_ = true;
                pending_changes_[table.first_page] = new_page;

                if (wals_[current_db_]->AppendPageRecord(table.first_page, new_page) != Status::OK ||
                    wals_[current_db_]->Flush() != Status::OK) {
                    std::cout << "ERROR: WAL write failed\n";
                    in_transaction_ = false;
                    pending_changes_.clear();
                    return Status::ERROR;
                }
                Status s = disk_managers_[current_db_]->WritePage(table.first_page, new_page);
                if (s != Status::OK) {
                    std::cout << "ERROR: Write failed\n";
                    in_transaction_ = false;
                    pending_changes_.clear();
                    return Status::ERROR;
                }
                buffer_pools_[current_db_]->Invalidate(table.first_page);
                in_transaction_ = false;
                pending_changes_.clear();
            }

            std::cout << "Row inserted\n";
            return Status::OK;
        }
    } else if (op == "UPDATE") {
        // UPDATE table SET col=val [WHERE ...];
        std::string table_name;
        ss >> table_name;
        if (!table_name.empty() && table_name.back() == ';') table_name.pop_back();

        if (current_db_.empty()) {
            std::cout << "ERROR: No database selected\n";
            return Status::ERROR;
        }

        if (tables_[current_db_].find(table_name) == tables_[current_db_].end()) {
            std::cout << "ERROR: Table does not exist\n";
            return Status::ERROR;
        }

        auto& table = tables_[current_db_][table_name];

        std::string set_keyword;
        ss >> set_keyword;
        if (set_keyword != "SET") {
            std::cout << "ERROR: Syntax: UPDATE table SET col=val [WHERE ...];\n";
            return Status::ERROR;
        }

        // Read rest of line for SET and optional WHERE
        std::string rest;
        std::getline(ss, rest);
        // find '=' in SET clause
        size_t eq_pos = rest.find('=');
        if (eq_pos == std::string::npos) {
            std::cout << "ERROR: Syntax error in SET clause\n";
            return Status::ERROR;
        }

        std::string col_to_update = rest.substr(0, eq_pos);
        // trim
        col_to_update.erase(0, col_to_update.find_first_not_of(" \t"));
        col_to_update.erase(col_to_update.find_last_not_of(" \t") + 1);

        std::string after_eq = rest.substr(eq_pos + 1);
        // split new value and optional WHERE
        std::string new_val_str;
        std::string where_clause;
        size_t where_pos = after_eq.find("WHERE");
        if (where_pos != std::string::npos) {
            new_val_str = after_eq.substr(0, where_pos);
            where_clause = after_eq.substr(where_pos + 5);
        } else {
            new_val_str = after_eq;
        }
        // trim
        new_val_str.erase(0, new_val_str.find_first_not_of(" \t"));
        new_val_str.erase(new_val_str.find_last_not_of(" \t;\n") + 1);
        if (!new_val_str.empty() && new_val_str.front() == '\'' && new_val_str.back() == '\'') {
            new_val_str = new_val_str.substr(1, new_val_str.size() - 2);
        }

        std::vector<WhereCondition> conditions;
        if (!where_clause.empty()) {
            conditions = ParseWhereClause(where_clause, table);
        }

        // find column index
        int col_idx = -1;
        for (size_t i = 0; i < table.columns.size(); ++i) {
            if (table.columns[i].name == col_to_update) { col_idx = static_cast<int>(i); break; }
        }
        if (col_idx == -1) {
            std::cout << "ERROR: Column does not exist\n";
            return Status::ERROR;
        }

        // Read page and update matching rows
        Page* page = buffer_pools_[current_db_]->GetPage(table.first_page);
        if (!page) { std::cout << "No data\n"; return Status::OK; }

        // Work from the most recent version of the page: pending in-transaction copy or the buffer pool page.
        Page base_page = (in_transaction_ && pending_changes_.count(table.first_page))
                             ? pending_changes_[table.first_page]
                             : *page;

        const char* data = base_page.data();
        std::stringstream data_ss(data);
        std::string line;
        Page new_page; std::memset(new_page.data(), 0, PAGE_SIZE);
        char* new_data = new_page.data();
        size_t offset = 0;
        int updated_count = 0;

        while (std::getline(data_ss, line)) {
            if (line.empty()) continue;
            Row row;
            std::stringstream row_ss(line);
            std::string val_str;
            while (row_ss >> val_str) {
                if (val_str.front() == '\'' && val_str.back() == '\'')
                    row.values.push_back(val_str.substr(1, val_str.size() - 2));
                else
                    row.values.push_back(std::stoi(val_str));
            }

            if (conditions.empty() || MatchesConditions(row, table, conditions)) {
                // update
                if (static_cast<size_t>(col_idx) < row.values.size()) {
                    if (table.columns[col_idx].type == DataType::INT) {
                        row.values[col_idx] = std::stoi(new_val_str);
                    } else {
                        row.values[col_idx] = new_val_str;
                    }
                    updated_count++;
                }
            }

            // serialize row back
            for (size_t i = 0; i < row.values.size(); ++i) {
                if (std::holds_alternative<int>(row.values[i])) {
                    std::snprintf(new_data + offset, PAGE_SIZE - offset, "%d ", std::get<int>(row.values[i]));
                } else {
                    std::snprintf(new_data + offset, PAGE_SIZE - offset, "'%s' ", std::get<std::string>(row.values[i]).c_str());
                }
                offset += std::strlen(new_data + offset);
            }
            std::snprintf(new_data + offset, PAGE_SIZE - offset, "\n");
            offset += 1;
        }

        // Persist according to transaction mode.
        if (in_transaction_) {
            pending_changes_[table.first_page] = new_page;
        } else {
            if (!autocommit_) {
                std::cout << "ERROR: Not in transaction. Use BEGIN; first or SET AUTOCOMMIT ON;\n";
                return Status::ERROR;
            }

            in_transaction_ = true;
            pending_changes_[table.first_page] = new_page;

            if (wals_[current_db_]->AppendPageRecord(table.first_page, new_page) != Status::OK ||
                wals_[current_db_]->Flush() != Status::OK) {
                std::cout << "WAL error\n";
                in_transaction_ = false;
                pending_changes_.clear();
                return Status::ERROR;
            }
            if (disk_managers_[current_db_]->WritePage(table.first_page, new_page) != Status::OK) {
                std::cout << "Write error\n";
                in_transaction_ = false;
                pending_changes_.clear();
                return Status::ERROR;
            }
            buffer_pools_[current_db_]->Invalidate(table.first_page);
            in_transaction_ = false;
            pending_changes_.clear();
        }

        std::cout << "Updated " << updated_count << " rows\n";
        return Status::OK;

    } else if (op == "DELETE") {
        // DELETE FROM table [WHERE ...];
        std::string from_keyword;
        ss >> from_keyword;
        if (from_keyword != "FROM") { std::cout << "ERROR: Syntax: DELETE FROM table [WHERE ...];\n"; return Status::ERROR; }
        if (current_db_.empty()) { std::cout << "ERROR: No database selected\n"; return Status::ERROR; }
        std::string table_name; ss >> table_name; if (!table_name.empty() && table_name.back() == ';') table_name.pop_back();
        if (tables_[current_db_].find(table_name) == tables_[current_db_].end()) { std::cout << "ERROR: Table does not exist\n"; return Status::ERROR; }

        auto& table = tables_[current_db_][table_name];
        std::vector<WhereCondition> conditions;
        std::string where_keyword; ss >> where_keyword;
        if (where_keyword == "WHERE") {
            std::string where_clause; std::getline(ss, where_clause);
            conditions = ParseWhereClause(where_clause, table);
        }

        Page* page = buffer_pools_[current_db_]->GetPage(table.first_page);
        if (!page) { std::cout << "No data\n"; return Status::OK; }

        // Use latest in-transaction version if present.
        Page base_page = (in_transaction_ && pending_changes_.count(table.first_page))
                             ? pending_changes_[table.first_page]
                             : *page;

        const char* data = base_page.data();
        std::stringstream data_ss(data);
        std::string line;
        Page new_page; std::memset(new_page.data(), 0, PAGE_SIZE);
        char* new_data = new_page.data();
        size_t offset = 0;
        int deleted_count = 0;

        while (std::getline(data_ss, line)) {
            if (line.empty()) continue;
            Row row; std::stringstream row_ss(line); std::string val_str;
            while (row_ss >> val_str) {
                if (val_str.front() == '\'' && val_str.back() == '\'') row.values.push_back(val_str.substr(1, val_str.size() - 2));
                else row.values.push_back(std::stoi(val_str));
            }

            if (conditions.empty() || MatchesConditions(row, table, conditions)) {
                deleted_count++; // skip
            } else {
                std::snprintf(new_data + offset, PAGE_SIZE - offset, "%s\n", line.c_str());
                offset += line.length() + 1;
            }
        }

        if (in_transaction_) {
            pending_changes_[table.first_page] = new_page;
        } else {
            if (!autocommit_) {
                std::cout << "ERROR: Not in transaction. Use BEGIN; first or SET AUTOCOMMIT ON;\n";
                return Status::ERROR;
            }

            in_transaction_ = true;
            pending_changes_[table.first_page] = new_page;

            if (wals_[current_db_]->AppendPageRecord(table.first_page, new_page) != Status::OK ||
                wals_[current_db_]->Flush() != Status::OK) {
                std::cout << "WAL error\n";
                in_transaction_ = false;
                pending_changes_.clear();
                return Status::ERROR;
            }
            if (disk_managers_[current_db_]->WritePage(table.first_page, new_page) != Status::OK) {
                std::cout << "Write error\n";
                in_transaction_ = false;
                pending_changes_.clear();
                return Status::ERROR;
            }
            buffer_pools_[current_db_]->Invalidate(table.first_page);
            in_transaction_ = false;
            pending_changes_.clear();
        }

        std::cout << "Deleted " << deleted_count << " rows\n";
        return Status::OK;

    } else if (op == "SELECT") {
        if (current_db_.empty()) {
            std::cout << "ERROR: No database selected. USE db_name;\n";
            return Status::ERROR;
        }
        std::string star_or_col;
        ss >> star_or_col;
        std::string from_keyword;
        ss >> from_keyword;
        if (from_keyword != "FROM") {
            std::cout << "ERROR: Syntax: SELECT * FROM table [WHERE ...] [LIMIT n] [OFFSET m];\n";
            return Status::ERROR;
        }
        std::string table_name;
        ss >> table_name;
        if (!table_name.empty() && table_name.back() == ';') table_name.pop_back();

        if (tables_[current_db_].find(table_name) == tables_[current_db_].end()) {
            std::cout << "ERROR: Table does not exist\n";
            return Status::ERROR;
        }

        auto& table = tables_[current_db_][table_name];

        std::string rest;
        std::getline(ss, rest);
        rest.erase(0, rest.find_first_not_of(" \t"));
        if (!rest.empty() && rest.back() == ';') rest.pop_back();

        std::vector<WhereCondition> conditions;
        int limit_val = -1;
        int offset_val = 0;

        size_t limit_pos = rest.find("LIMIT");
        size_t offset_pos = rest.find("OFFSET");
        // Where clause is everything before first of LIMIT/OFFSET
        size_t where_end = rest.size();
        if (limit_pos != std::string::npos) where_end = std::min(where_end, limit_pos);
        if (offset_pos != std::string::npos) where_end = std::min(where_end, offset_pos);
        std::string where_part = rest.substr(0, where_end);
        where_part.erase(0, where_part.find_first_not_of(" \t"));
        if (!where_part.empty() && where_part.substr(0, 5) == "WHERE") {
            where_part = where_part.substr(5);
            where_part.erase(0, where_part.find_first_not_of(" \t"));
            conditions = ParseWhereClause(where_part, table);
        }
        if (limit_pos != std::string::npos) {
            std::string limit_rest = rest.substr(limit_pos + 5);
            limit_rest.erase(0, limit_rest.find_first_not_of(" \t"));
            size_t space = limit_rest.find_first_of(" \t");
            std::string num = (space == std::string::npos) ? limit_rest : limit_rest.substr(0, space);
            if (!num.empty()) { try { limit_val = std::stoi(num); } catch (...) {} }
        }
        if (offset_pos != std::string::npos) {
            std::string off_rest = rest.substr(offset_pos + 6);
            off_rest.erase(0, off_rest.find_first_not_of(" \t"));
            size_t sp = off_rest.find_first_of(" \t;");
            std::string onum = (sp == std::string::npos) ? off_rest : off_rest.substr(0, sp);
            if (!onum.empty()) { try { offset_val = std::stoi(onum); } catch (...) {} }
        }

        Page* page = buffer_pools_[current_db_]->GetPage(table.first_page);
        if (!page) {
            std::cout << "(0 rows)\n";
            return Status::OK;
        }

        // SELECT should see uncommitted changes inside the current transaction, like a real DBMS.
        const Page& source_page = (in_transaction_ && pending_changes_.count(table.first_page))
                                      ? pending_changes_[table.first_page]
                                      : *page;

        const char* data = source_page.data();
        std::stringstream data_ss(data);
        std::string line;
        std::vector<Row> matched;

        while (std::getline(data_ss, line)) {
            if (line.empty()) continue;
            Row row;
            std::stringstream row_ss(line);
            std::string val_str;
            while (row_ss >> val_str) {
                if (val_str.size() >= 2 && val_str.front() == '\'' && val_str.back() == '\'') {
                    row.values.push_back(val_str.substr(1, val_str.size() - 2));
                } else {
                    try {
                        row.values.push_back(std::stoi(val_str));
                    } catch (...) {
                        row.values.push_back(val_str);
                    }
                }
            }
            if (conditions.empty() || MatchesConditions(row, table, conditions)) {
                matched.push_back(std::move(row));
            }
        }

        // Apply OFFSET and LIMIT
        if (offset_val > 0 && static_cast<size_t>(offset_val) >= matched.size()) {
            matched.clear();
        } else if (offset_val > 0) {
            matched.erase(matched.begin(), matched.begin() + std::min(static_cast<size_t>(offset_val), matched.size()));
        }
        if (limit_val >= 0 && static_cast<size_t>(limit_val) < matched.size()) {
            matched.resize(static_cast<size_t>(limit_val));
        }

        // Tabular output: header + rows
        if (matched.empty()) {
            std::cout << "(0 rows)\n";
            return Status::OK;
        }
        std::vector<size_t> widths(table.columns.size());
        for (size_t c = 0; c < table.columns.size(); ++c) {
            widths[c] = table.columns[c].name.size();
        }
        for (const auto& r : matched) {
            for (size_t c = 0; c < r.values.size() && c < widths.size(); ++c) {
                size_t len = 0;
                if (std::holds_alternative<int>(r.values[c])) {
                    len = static_cast<size_t>(std::snprintf(nullptr, 0, "%d", std::get<int>(r.values[c])));
                } else {
                    len = std::get<std::string>(r.values[c]).size();
                }
                if (len > widths[c]) widths[c] = len;
            }
        }
        auto print_cell = [](const Value& v, size_t w) {
            if (std::holds_alternative<int>(v)) {
                std::cout << std::setw(static_cast<int>(w)) << std::get<int>(v);
            } else {
                std::string s = std::get<std::string>(v);
                std::cout << s;
                for (size_t i = s.size(); i < w; ++i) std::cout << ' ';
            }
        };
        for (size_t c = 0; c < table.columns.size(); ++c) {
            if (c) std::cout << " | ";
            print_cell(Value(table.columns[c].name), widths[c]);
        }
        std::cout << "\n";
        for (size_t c = 0; c < table.columns.size(); ++c) {
            if (c) std::cout << "-+-";
            for (size_t i = 0; i < widths[c]; ++i) std::cout << "-";
        }
        std::cout << "\n";
        for (const auto& r : matched) {
            for (size_t c = 0; c < r.values.size() && c < table.columns.size(); ++c) {
                if (c) std::cout << " | ";
                print_cell(r.values[c], widths[c]);
            }
            std::cout << "\n";
        }
        std::cout << "(" << matched.size() << " row(s))\n";
        return Status::OK;
    } else if (!op.empty()) {
        std::cout << "ERROR: Unknown command: " << op << "\n";
    }

    return Status::OK;
}

// Helper methods for WHERE clause
bool Database::EvaluateCondition(const Row& row, const Table& table, const WhereCondition& condition) const {
    // Find column index
    size_t col_idx = 0;
    bool found = false;
    for (size_t i = 0; i < table.columns.size(); ++i) {
        if (table.columns[i].name == condition.column_name) {
            col_idx = i;
            found = true;
            break;
        }
    }
    
    if (!found || col_idx >= row.values.size()) {
        return false;
    }
    
    const Value& col_val = row.values[col_idx];
    const Value& cond_val = condition.value;
    
    // Compare based on operator (like PostgreSQL query executor)
    switch (condition.op) {
        case ComparisonOp::EQ: {
            if (std::holds_alternative<int>(col_val) && std::holds_alternative<int>(cond_val)) {
                return std::get<int>(col_val) == std::get<int>(cond_val);
            } else if (std::holds_alternative<std::string>(col_val) && std::holds_alternative<std::string>(cond_val)) {
                return std::get<std::string>(col_val) == std::get<std::string>(cond_val);
            }
            return false;
        }
        case ComparisonOp::NE: {
            if (std::holds_alternative<int>(col_val) && std::holds_alternative<int>(cond_val)) {
                return std::get<int>(col_val) != std::get<int>(cond_val);
            } else if (std::holds_alternative<std::string>(col_val) && std::holds_alternative<std::string>(cond_val)) {
                return std::get<std::string>(col_val) != std::get<std::string>(cond_val);
            }
            return false;
        }
        case ComparisonOp::LT: {
            if (std::holds_alternative<int>(col_val) && std::holds_alternative<int>(cond_val)) {
                return std::get<int>(col_val) < std::get<int>(cond_val);
            }
            return false;
        }
        case ComparisonOp::LE: {
            if (std::holds_alternative<int>(col_val) && std::holds_alternative<int>(cond_val)) {
                return std::get<int>(col_val) <= std::get<int>(cond_val);
            }
            return false;
        }
        case ComparisonOp::GT: {
            if (std::holds_alternative<int>(col_val) && std::holds_alternative<int>(cond_val)) {
                return std::get<int>(col_val) > std::get<int>(cond_val);
            }
            return false;
        }
        case ComparisonOp::GE: {
            if (std::holds_alternative<int>(col_val) && std::holds_alternative<int>(cond_val)) {
                return std::get<int>(col_val) >= std::get<int>(cond_val);
            }
            return false;
        }
    }
    
    return false;
}

bool Database::MatchesConditions(const Row& row, const Table& table, const std::vector<WhereCondition>& conditions) const {
    // All conditions must be true (AND logic, like in SQL)
    for (const auto& cond : conditions) {
        if (!EvaluateCondition(row, table, cond)) {
            return false;
        }
    }
    return true;
}

ComparisonOp Database::ParseOperator(const std::string& op_str) const {
    if (op_str == "=") return ComparisonOp::EQ;
    if (op_str == "!=") return ComparisonOp::NE;
    if (op_str == "<") return ComparisonOp::LT;
    if (op_str == "<=") return ComparisonOp::LE;
    if (op_str == ">") return ComparisonOp::GT;
    if (op_str == ">=") return ComparisonOp::GE;
    return ComparisonOp::EQ;
}

std::vector<WhereCondition> Database::ParseWhereClause(const std::string& where_str, const Table& table) const {
    std::vector<WhereCondition> conditions;
    
    // Simple parser: "col1 = val1 AND col2 > val2"
    std::stringstream ss(where_str);
    std::string col_name, op_str, val_str;
    
    while (ss >> col_name >> op_str >> val_str) {
        // Remove 'AND' keyword if present
        std::string and_keyword;
        if (val_str == "AND") {
            ss >> col_name >> op_str >> val_str;
        } else if (!val_str.empty() && val_str.back() == ';') {
            val_str.pop_back();
        }
        
        // Convert value to appropriate type based on column type
        Value value;
        auto col_it = std::find_if(table.columns.begin(), table.columns.end(),
                                   [&col_name](const Column& c) { return c.name == col_name; });
        
        if (col_it != table.columns.end()) {
            if (col_it->type == DataType::INT) {
                value = std::stoi(val_str);
            } else {
                if (val_str.front() == '\'' && val_str.back() == '\'') {
                    value = val_str.substr(1, val_str.size() - 2);
                } else {
                    value = val_str;
                }
            }
            
            conditions.push_back({col_name, ParseOperator(op_str), value});
        }
    }
    
    return conditions;
}

Status Database::SaveCatalog(const std::string& db_name) {
    auto it_dm = disk_managers_.find(db_name);
    auto it_tables = tables_.find(db_name);
    if (it_dm == disk_managers_.end() || it_tables == tables_.end()) return Status::ERROR;
    DiskManager* dm = it_dm->second.get();
    const auto& tbl_map = it_tables->second;
    std::ostringstream os;
    os << "CATALOG\n";
    for (const auto& kv : tbl_map) {
        const Table& t = kv.second;
        os << "TABLE " << t.name << " " << t.first_page;
        for (const auto& col : t.columns) {
            os << " " << col.name << " " << (col.type == DataType::INT ? "INT" : "VARCHAR");
        }
        os << "\n";
    }
    std::string cat_str = os.str();
    Page catalog{};
    size_t copy_len = std::min(cat_str.size(), PAGE_SIZE - 1);
    std::memcpy(catalog.data(), cat_str.data(), copy_len);
    catalog[copy_len] = '\0';
    if (wals_.count(db_name) && wals_[db_name]) {
        if (wals_[db_name]->AppendPageRecord(kCatalogPageId, catalog) != Status::OK ||
            wals_[db_name]->Flush() != Status::OK) return Status::ERROR;
    }
    if (dm->WritePage(kCatalogPageId, catalog) != Status::OK) return Status::ERROR;
    if (buffer_pools_.count(db_name) && buffer_pools_[db_name])
        buffer_pools_[db_name]->Invalidate(kCatalogPageId);
    return Status::OK;
}

Status Database::LoadCatalog(const std::string& db_name) {
    auto it_dm = disk_managers_.find(db_name);
    if (it_dm == disk_managers_.end()) return Status::ERROR;
    if (it_dm->second->GetTotalPages() <= kCatalogPageId) return Status::OK;
    Page catalog{};
    if (it_dm->second->ReadPage(kCatalogPageId, catalog) != Status::OK) return Status::OK;
    std::stringstream ss(std::string(catalog.data(), PAGE_SIZE));
    std::string line;
    if (!std::getline(ss, line) || line != "CATALOG") return Status::OK;
    tables_[db_name].clear();
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::stringstream ls(line);
        std::string tok, table_name, col_name, type_str;
        page_id_t first_page = 0;
        if (!(ls >> tok) || tok != "TABLE") continue;
        if (!(ls >> table_name >> first_page)) continue;
        Table t;
        t.name = table_name;
        t.first_page = first_page;
        while (ls >> col_name >> type_str) {
            DataType dt = (type_str == "INT") ? DataType::INT : DataType::VARCHAR;
            t.columns.push_back({col_name, dt});
        }
        if (!t.columns.empty()) tables_[db_name][table_name] = std::move(t);
    }
    return Status::OK;
}