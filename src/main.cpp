#include <iostream>
#include <string>
#include <csignal>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <sstream>

#include "datyredb/sql/lexer.h"
#include "datyredb/sql/parser.h"

// Conditionally include network headers if they exist
#ifdef HAS_NETWORK
#include "datyredb/network/server.h"
#include "datyredb/network/http_server.h"
#endif

#ifdef HAS_CURL
#include "datyredb/network/telegram_bot.h"
#endif

using namespace datyredb;

// ===== FAKE DATABASE (временная заглушка вместо каталога) =====
namespace fake_db {
    // Простое хранилище данных в памяти
    struct Table {
        std::string name;
        std::vector<std::string> columns;
        std::vector<std::vector<std::string>> rows;
    };
    
    static std::map<std::string, Table> tables;
    static bool initialized = false;
    
    void init() {
        if (initialized) return;
        initialized = true;
        
        // Создаем тестовую таблицу users
        Table users;
        users.name = "users";
        users.columns = {"id", "name", "email", "age"};
        users.rows = {
            {"1", "Alice", "alice@example.com", "30"},
            {"2", "Bob", "bob@example.com", "25"},
            {"3", "Charlie", "charlie@example.com", "35"}
        };
        tables["users"] = users;
        
        // Создаем тестовую таблицу products
        Table products;
        products.name = "products";
        products.columns = {"id", "name", "price"};
        products.rows = {
            {"1", "Laptop", "999.99"},
            {"2", "Mouse", "25.99"},
            {"3", "Keyboard", "79.99"}
        };
        tables["products"] = products;
    }
    
    std::vector<std::string> get_all_tables() {
        std::vector<std::string> result;
        for (const auto& [name, _] : tables) {
            result.push_back(name);
        }
        return result;
    }
    
    Table* get_table(const std::string& name) {
        auto it = tables.find(name);
        if (it != tables.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    bool create_table(const std::string& name, const std::vector<std::string>& columns) {
        if (tables.find(name) != tables.end()) {
            return false; // Table already exists
        }
        Table t;
        t.name = name;
        t.columns = columns;
        tables[name] = t;
        return true;
    }
    
    bool drop_table(const std::string& name) {
        return tables.erase(name) > 0;
    }
    
    bool insert_row(const std::string& table_name, const std::vector<std::string>& values) {
        auto* table = get_table(table_name);
        if (!table) return false;
        
        if (values.size() != table->columns.size()) {
            return false; // Column count mismatch
        }
        
        table->rows.push_back(values);
        return true;
    }
}

// Global server instances for signal handling
#ifdef HAS_NETWORK
std::unique_ptr<network::Server> g_tcp_server;
std::unique_ptr<network::HttpServer> g_http_server;
#endif

#ifdef HAS_CURL
std::unique_ptr<network::TelegramBot> g_telegram_bot;
#endif

// Signal handler
void signal_handler(int signal) {
    std::cout << "\n[INFO] Shutting down...\n";
    
#ifdef HAS_NETWORK
    if (g_tcp_server) g_tcp_server->stop();
    if (g_http_server) g_http_server->stop();
#endif
    
#ifdef HAS_CURL
    if (g_telegram_bot) g_telegram_bot->stop();
#endif
    
    exit(0);
}

// Simple QueryResult structure (если нет network/types.h)
struct SimpleQueryResult {
    bool success{false};
    std::string error_message;
    std::vector<std::string> column_names;
    std::vector<std::vector<std::string>> rows;
    size_t rows_affected{0};
    std::chrono::microseconds execution_time{0};
    
    std::string to_string() const {
        if (!success) {
            return "Error: " + error_message;
        }
        
        if (rows.empty()) {
            return "Query executed successfully. " + std::to_string(rows_affected) + " rows affected.";
        }
        
        std::stringstream ss;
        
        // Print column names
        for (size_t i = 0; i < column_names.size(); ++i) {
            if (i > 0) ss << " | ";
            ss << column_names[i];
        }
        ss << "\n";
        
        // Print separator
        for (size_t i = 0; i < column_names.size(); ++i) {
            if (i > 0) ss << "-+-";
            for (size_t j = 0; j < column_names[i].size(); ++j) {
                ss << "-";
            }
        }
        ss << "\n";
        
        // Print rows
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) ss << " | ";
                ss << row[i];
            }
            ss << "\n";
        }
        
        ss << "\n(" << rows.size() << " rows in " 
           << execution_time.count() / 1000.0 << " ms)";
        
        return ss.str();
    }
    
    std::string to_json() const {
        std::stringstream ss;
        ss << "{";
        ss << "\"success\": " << (success ? "true" : "false") << ", ";
        
        if (!success) {
            ss << "\"error\": \"" << error_message << "\"";
        } else {
            ss << "\"columns\": [";
            for (size_t i = 0; i < column_names.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << "\"" << column_names[i] << "\"";
            }
            ss << "], ";
            
            ss << "\"rows\": [";
            for (size_t r = 0; r < rows.size(); ++r) {
                if (r > 0) ss << ", ";
                ss << "[";
                for (size_t c = 0; c < rows[r].size(); ++c) {
                    if (c > 0) ss << ", ";
                    ss << "\"" << rows[r][c] << "\"";
                }
                ss << "]";
            }
            ss << "], ";
            
            ss << "\"rows_affected\": " << rows_affected << ", ";
            ss << "\"execution_time_ms\": " << (execution_time.count() / 1000.0);
        }
        
        ss << "}";
        return ss.str();
    }
};

// Execute SQL query (упрощенная версия)
SimpleQueryResult execute_query(const std::string& query) {
    SimpleQueryResult result;
    auto start = std::chrono::steady_clock::now();
    
    try {
        // Parse SQL
        sql::Lexer lexer(query);
        auto tokens = lexer.tokenize();
        
        sql::Parser parser(std::move(tokens));
        auto stmt = parser.parse();
        
        // Process based on statement type
        if (stmt->get_type() == sql::Statement::Type::SELECT) {
            auto* select_stmt = dynamic_cast<sql::SelectStatement*>(stmt.get());
            
            // Simple SELECT * FROM table implementation
            if (select_stmt->table_name_.empty()) {
                throw std::runtime_error("No table specified");
            }
            
            auto* table = fake_db::get_table(select_stmt->table_name_);
            if (!table) {
                throw std::runtime_error("Table not found: " + select_stmt->table_name_);
            }
            
            result.column_names = table->columns;
            result.rows = table->rows;
            
            // Simple WHERE clause filtering (если есть)
            if (select_stmt->where_clause_) {
                // TODO: Implement WHERE filtering
                // Пока просто возвращаем все строки
            }
            
            result.success = true;
            
        } else if (stmt->get_type() == sql::Statement::Type::INSERT) {
            auto* insert_stmt = dynamic_cast<sql::InsertStatement*>(stmt.get());
            
            auto* table = fake_db::get_table(insert_stmt->table_name_);
            if (!table) {
                throw std::runtime_error("Table not found: " + insert_stmt->table_name_);
            }
            
            // Collect values from the INSERT statement
            std::vector<std::string> values;
            for (const auto& val_expr : insert_stmt->values_) {
                // Simplified: just use the string representation
                values.push_back(val_expr->to_string());
            }
            
            if (fake_db::insert_row(insert_stmt->table_name_, values)) {
                result.success = true;
                result.rows_affected = 1;
            } else {
                throw std::runtime_error("Failed to insert row");
            }
            
        } else if (stmt->get_type() == sql::Statement::Type::CREATE_TABLE) {
            auto* create_stmt = dynamic_cast<sql::CreateTableStatement*>(stmt.get());
            
            std::vector<std::string> columns;
            for (const auto& col_def : create_stmt->columns_) {
                columns.push_back(col_def.name);
            }
            
            if (fake_db::create_table(create_stmt->table_name_, columns)) {
                result.success = true;
                result.rows_affected = 0;
            } else {
                throw std::runtime_error("Table already exists: " + create_stmt->table_name_);
            }
            
        } else if (stmt->get_type() == sql::Statement::Type::DELETE) {
            auto* delete_stmt = dynamic_cast<sql::DeleteStatement*>(stmt.get());
            
            auto* table = fake_db::get_table(delete_stmt->table_name_);
            if (!table) {
                throw std::runtime_error("Table not found: " + delete_stmt->table_name_);
            }
            
            // Simple DELETE (удаляем все строки если нет WHERE)
            size_t deleted = table->rows.size();
            table->rows.clear();
            
            result.success = true;
            result.rows_affected = deleted;
            
        } else if (stmt->get_type() == sql::Statement::Type::UPDATE) {
            auto* update_stmt = dynamic_cast<sql::UpdateStatement*>(stmt.get());
            
            auto* table = fake_db::get_table(update_stmt->table_name_);
            if (!table) {
                throw std::runtime_error("Table not found: " + update_stmt->table_name_);
            }
            
            // TODO: Implement UPDATE logic
            result.success = true;
            result.rows_affected = table->rows.size();
            
        } else {
            throw std::runtime_error("Unsupported statement type");
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    return result;
}

void print_banner() {
    std::cout << R"(
    ____        __           ____  ____ 
   / __ \____ _/ /___  _____|  _ \|  _ \
  / / / / __ `/ __/ / / / __|  | | |_) |
 / /_/ / /_/ / /_/ /_/ / |  | |_| |  _ <
/_____/\__,_/\__/\__, /|_|  |____/|_|_\_\
                /____/                    
    )" << "\n";
    std::cout << "    DatyreDB v0.1.0 - Professional SQL Database\n";
    std::cout << "    ============================================\n\n";
}

void print_help() {
    std::cout << "SQL Commands:\n";
    std::cout << "  SELECT * FROM table_name\n";
    std::cout << "  INSERT INTO table_name VALUES (...)\n";
    std::cout << "  UPDATE table_name SET col = val\n";
    std::cout << "  DELETE FROM table_name\n";
    std::cout << "  CREATE TABLE table_name (col1 type, col2 type, ...)\n";
    std::cout << "  DROP TABLE table_name\n\n";
    
    std::cout << "Meta Commands:\n";
    std::cout << "  \\dt         - List all tables\n";
    std::cout << "  \\d <table>  - Describe table\n";
    std::cout << "  \\help or \\h - Show this help\n";
    std::cout << "  \\quit or \\q - Exit\n\n";
    
    std::cout << "Command Line Options:\n";
    std::cout << "  --help              Show this help message\n";
    std::cout << "  --port <port>       TCP server port (default: 5432)\n";
    std::cout << "  --http-port <port>  HTTP API port (default: 8080)\n";
    std::cout << "  --no-http           Disable HTTP API\n";
    
#ifdef HAS_CURL
    std::cout << "  --telegram <token>  Enable Telegram bot with token\n";
    std::cout << "  --telegram-admin <id> Add admin user ID for Telegram\n";
#endif
    
    std::cout << "  --daemon            Run as daemon (no REPL)\n\n";
}

// Setup HTTP routes (if network support is enabled)
#ifdef HAS_NETWORK
void setup_api_routes(network::HttpServer& http) {
    http.enable_cors();
    
    // Health check
    http.get("/health", [](const network::HttpRequest&) {
        return network::HttpResponse::ok(R"({"status": "ok", "version": "0.1.0"})");
    });
    
    // Execute query
    http.post("/api/query", [](const network::HttpRequest& req) {
        auto body = req.body;
        
        // Simple JSON parsing (find "query" field)
        size_t query_pos = body.find("\"query\"");
        if (query_pos == std::string::npos) {
            return network::HttpResponse::bad_request("Missing 'query' field");
        }
        
        size_t start = body.find("\"", query_pos + 7) + 1;
        size_t end = body.find("\"", start);
        std::string query = body.substr(start, end - start);
        
        auto result = execute_query(query);
        return network::HttpResponse::ok(result.to_json());
    });
    
    // List tables
    http.get("/api/tables", [](const network::HttpRequest&) {
        auto tables = fake_db::get_all_tables();
        std::string json = R"({"tables": [)";
        for (size_t i = 0; i < tables.size(); ++i) {
            if (i > 0) json += ",";
            json += "\"" + tables[i] + "\"";
        }
        json += "]}";
        return network::HttpResponse::ok(json);
    });
    
    // Get table info
    http.get("/api/tables/:name", [](const network::HttpRequest& req) {
        std::string table_name = req.get_param("name");
        auto* table = fake_db::get_table(table_name);
        
        if (!table) {
            return network::HttpResponse::not_found("Table not found");
        }
        
        std::string json = "{";
        json += "\"name\": \"" + table->name + "\", ";
        json += "\"columns\": [";
        for (size_t i = 0; i < table->columns.size(); ++i) {
            if (i > 0) json += ", ";
            json += "\"" + table->columns[i] + "\"";
        }
        json += "], ";
        json += "\"row_count\": " + std::to_string(table->rows.size());
        json += "}";
        
        return network::HttpResponse::ok(json);
    });
}
#endif

// Setup Telegram commands (if telegram support is enabled)
#ifdef HAS_CURL
void setup_telegram_commands(network::TelegramBot& bot) {
    // Set query executor
    bot.set_query_executor([](const std::string& query) -> network::QueryResult {
        auto result = execute_query(query);
        
        // Convert SimpleQueryResult to network::QueryResult
        network::QueryResult net_result;
        net_result.success = result.success;
        net_result.error_message = result.error_message;
        net_result.column_names = result.column_names;
        net_result.rows = result.rows;
        net_result.rows_affected = result.rows_affected;
        net_result.execution_time = result.execution_time;
        
        return net_result;
    });
    
    // List tables command
    bot.register_command("tables", [](const network::TelegramMessage&, const std::vector<std::string>&) {
        auto tables = fake_db::get_all_tables();
        if (tables.empty()) {
            return std::string("📭 No tables found");
        }
        
        std::string response = "📋 *Tables:*\n";
        for (const auto& table : tables) {
            response += "• `" + table + "`\n";
        }
        return response;
    });
    
    // Schema command
    bot.register_command("schema", [](const network::TelegramMessage&, const std::vector<std::string>& args) {
        if (args.empty()) {
            return std::string("Usage: /schema <table_name>");
        }
        
        auto* table = fake_db::get_table(args[0]);
        if (!table) {
            return std::string("❌ Table not found: " + args[0]);
        }
        
        std::string response = "📊 *Table: " + table->name + "*\n";
        response += "Columns:\n";
        for (const auto& col : table->columns) {
            response += "• " + col + "\n";
        }
        response += "\nRows: " + std::to_string(table->rows.size());
        
        return response;
    });
    
    // Stats command (admin only)
    bot.register_command("stats", [](const network::TelegramMessage&, const std::vector<std::string>&) {
        auto tables = fake_db::get_all_tables();
        size_t total_rows = 0;
        
        for (const auto& table_name : tables) {
            auto* table = fake_db::get_table(table_name);
            if (table) {
                total_rows += table->rows.size();
            }
        }
        
        return "📊 *Database Statistics:*\n"
               "• Tables: " + std::to_string(tables.size()) + "\n"
               "• Total Rows: " + std::to_string(total_rows) + "\n"
               "• Version: 0.1.0";
    }, true);
}
#endif

int main(int argc, char* argv[]) {
    // Initialize fake database
    fake_db::init();
    
    // Parse command line arguments
    bool enable_repl = true;
    bool daemon_mode = false;
    
#ifdef HAS_NETWORK
    network::ServerConfig config;
#else
    // Simple config structure if network is not available
    struct {
        uint16_t tcp_port = 5432;
        uint16_t http_port = 8080;
        bool enable_http = true;
        bool enable_telegram = false;
        std::string telegram_bot_token;
        std::vector<int64_t> telegram_admin_ids;
    } config;
#endif
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_banner();
            print_help();
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            config.tcp_port = std::stoi(argv[++i]);
        } else if (arg == "--http-port" && i + 1 < argc) {
            config.http_port = std::stoi(argv[++i]);
        } else if (arg == "--no-http") {
            config.enable_http = false;
        } else if (arg == "--telegram" && i + 1 < argc) {
#ifdef HAS_CURL
            config.enable_telegram = true;
            config.telegram_bot_token = argv[++i];
#else
            std::cout << "[WARNING] Telegram support not compiled in (CURL not found)\n";
            ++i; // Skip token
#endif
        } else if (arg == "--telegram-admin" && i + 1 < argc) {
#ifdef HAS_CURL
            config.telegram_admin_ids.push_back(std::stoll(argv[++i]));
#else
            ++i; // Skip ID
#endif
        } else if (arg == "--daemon") {
            daemon_mode = true;
            enable_repl = false;
        }
    }
    
    print_banner();
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
#ifdef HAS_NETWORK
    // Start TCP server
    try {
        g_tcp_server = std::make_unique<network::Server>(config);
        g_tcp_server->set_on_query([](const std::string& query, network::Session&) {
            auto result = execute_query(query);
            
            // Convert SimpleQueryResult to network::QueryResult
            network::QueryResult net_result;
            net_result.success = result.success;
            net_result.error_message = result.error_message;
            net_result.column_names = result.column_names;
            net_result.rows = result.rows;
            net_result.rows_affected = result.rows_affected;
            net_result.execution_time = result.execution_time;
            
            return net_result;
        });
        
        if (g_tcp_server->start()) {
            std::cout << "[INFO] TCP Server listening on port " << config.tcp_port << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to start TCP server: " << e.what() << "\n";
    }
    
    // Start HTTP server
    if (config.enable_http) {
        try {
            g_http_server = std::make_unique<network::HttpServer>(config.http_port);
            setup_api_routes(*g_http_server);
            
            if (g_http_server->start()) {
                std::cout << "[INFO] HTTP API listening on port " << config.http_port << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to start HTTP server: " << e.what() << "\n";
        }
    }
#endif
    
#ifdef HAS_CURL
    // Start Telegram bot
    if (config.enable_telegram && !config.telegram_bot_token.empty()) {
        try {
            network::TelegramConfig tg_config;
            tg_config.bot_token = config.telegram_bot_token;
            tg_config.admin_user_ids = config.telegram_admin_ids;
            
            g_telegram_bot = std::make_unique<network::TelegramBot>(tg_config);
            setup_telegram_commands(*g_telegram_bot);
            
            if (g_telegram_bot->start()) {
                std::cout << "[INFO] Telegram bot started\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to start Telegram bot: " << e.what() << "\n";
        }
    }
#endif
    
    std::cout << "\n";
    
    // REPL mode
    if (enable_repl && !daemon_mode) {
        std::cout << "Type '\\help' for available commands, '\\quit' to exit\n\n";
        
        while (true) {
            std::cout << "datyredb> ";
            
            std::string query;
            std::getline(std::cin, query);
            
            if (query == "\\quit" || query == "\\q" || query == "exit") {
                break;
            }
            
            if (query.empty()) continue;
            
            if (query == "\\help" || query == "\\h") {
                print_help();
                continue;
            }
            
            if (query == "\\dt") {
                auto tables = fake_db::get_all_tables();
                std::cout << "List of tables:\n";
                for (const auto& t : tables) {
                    auto* table = fake_db::get_table(t);
                    std::cout << "  " << t << " (" << table->rows.size() << " rows)\n";
                }
                std::cout << "\n";
                continue;
            }
            
            if (query.substr(0, 3) == "\\d ") {
                std::string table_name = query.substr(3);
                auto* table = fake_db::get_table(table_name);
                if (table) {
                    std::cout << "Table: " << table_name << "\n";
                    std::cout << "Columns:\n";
                    for (const auto& col : table->columns) {
                        std::cout << "  - " << col << "\n";
                    }
                    std::cout << "Rows: " << table->rows.size() << "\n\n";
                } else {
                    std::cout << "Table not found: " << table_name << "\n\n";
                }
                continue;
            }
            
            // Execute SQL
            auto result = execute_query(query);
            std::cout << result.to_string() << "\n\n";
        }
    } else if (daemon_mode) {
        std::cout << "[INFO] Running in daemon mode. Press Ctrl+C to stop.\n";
        
        // Wait indefinitely
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
#ifdef HAS_NETWORK
            // Check if servers are still running
            if (g_tcp_server && !g_tcp_server->is_running()) {
                break;
            }
#endif
        }
    }
    
    // Cleanup
#ifdef HAS_CURL
    if (g_telegram_bot) g_telegram_bot->stop();
#endif
    
#ifdef HAS_NETWORK
    if (g_http_server) g_http_server->stop();
    if (g_tcp_server) g_tcp_server->stop();
#endif
    
    std::cout << "[INFO] DatyreDB shutdown complete\n";
    return 0;
}
