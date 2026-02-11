#include "datyredb/network/telegram_bot.h"
#include <curl/curl.h>
#include <sstream>
#include <iostream>
#include <regex>

namespace datyredb::network {

// CURL write callback
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

TelegramBot::TelegramBot(const TelegramConfig& config) 
    : config_(config)
    , api_base_url_("https://api.telegram.org/bot" + config.bot_token + "/") {
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    // Register default commands
    register_command("start", [](const TelegramMessage& msg, const std::vector<std::string>&) {
        return "🗄️ *DatyreDB Telegram Bot*\n\n"
               "Available commands:\n"
               "/query <SQL> - Execute SQL query\n"
               "/tables - List all tables\n"
               "/schema <table> - Show table schema\n"
               "/stats - Show database statistics\n"
               "/help - Show this help";
    });
    
    register_command("help", [](const TelegramMessage& msg, const std::vector<std::string>&) {
        return "📖 *DatyreDB Help*\n\n"
               "*Query Commands:*\n"
               "`/query SELECT * FROM users` - Run SQL\n"
               "`/tables` - List tables\n"
               "`/schema users` - Table schema\n\n"
               "*Admin Commands:*\n"
               "`/stats` - DB statistics\n"
               "`/connections` - Active connections";
    });
}

TelegramBot::~TelegramBot() {
    stop();
    curl_global_cleanup();
}

bool TelegramBot::start() {
    running_ = true;
    
    if (config_.use_webhook) {
        // Webhook mode - set webhook URL
        std::string params = "url=" + config_.webhook_url;
        make_api_request("setWebhook", params);
        std::cout << "[INFO] Telegram bot started in webhook mode\n";
    } else {
        // Polling mode
        make_api_request("deleteWebhook");
        polling_thread_ = std::thread(&TelegramBot::polling_loop, this);
        std::cout << "[INFO] Telegram bot started in polling mode\n";
    }
    
    return true;
}

void TelegramBot::stop() {
    running_ = false;
    if (polling_thread_.joinable()) {
        polling_thread_.join();
    }
}

bool TelegramBot::send_message(int64_t chat_id, const std::string& text, bool markdown) {
    std::string escaped_text = text;
    // URL encode special characters
    
    std::string params = "chat_id=" + std::to_string(chat_id) + 
                        "&text=" + escaped_text;
    if (markdown) {
        params += "&parse_mode=Markdown";
    }
    
    std::string response = make_api_request("sendMessage", params);
    return response.find("\"ok\":true") != std::string::npos;
}

bool TelegramBot::send_code_block(int64_t chat_id, const std::string& code, const std::string& language) {
    std::string message = "```" + language + "\n" + code + "\n```";
    return send_message(chat_id, message, true);
}

bool TelegramBot::send_table(int64_t chat_id, const QueryResult& result) {
    if (!result.success) {
        return send_message(chat_id, "❌ Error: " + result.error_message);
    }
    
    if (result.rows.empty()) {
        return send_message(chat_id, "✅ Query executed. No rows returned.\n"
                                     "Rows affected: " + std::to_string(result.rows_affected));
    }
    
    std::ostringstream oss;
    oss << "```\n";
    
    // Header
    for (size_t i = 0; i < result.column_names.size(); ++i) {
        if (i > 0) oss << " | ";
        oss << result.column_names[i];
    }
    oss << "\n";
    
    // Separator
    for (size_t i = 0; i < result.column_names.size(); ++i) {
        if (i > 0) oss << "-+-";
        oss << std::string(result.column_names[i].size(), '-');
    }
    oss << "\n";
    
    // Rows (limit to 20 for Telegram message size)
    size_t max_rows = std::min(result.rows.size(), size_t(20));
    for (size_t r = 0; r < max_rows; ++r) {
        for (size_t c = 0; c < result.rows[r].size(); ++c) {
            if (c > 0) oss << " | ";
            oss << result.rows[r][c];
        }
        oss << "\n";
    }
    
    if (result.rows.size() > 20) {
        oss << "... and " << (result.rows.size() - 20) << " more rows\n";
    }
    
    oss << "```\n";
    oss << "✅ " << result.rows.size() << " rows in " 
        << result.execution_time.count() / 1000.0 << "ms";
    
    return send_message(chat_id, oss.str(), true);
}

void TelegramBot::register_command(const std::string& command, CommandHandler handler, bool admin_only) {
    std::lock_guard lock(commands_mutex_);
    commands_[command] = {std::move(handler), admin_only};
}

HttpResponse TelegramBot::handle_webhook(const HttpRequest& req) {
    process_update(req.body);
    return HttpResponse::ok(R"({"ok": true})");
}

void TelegramBot::polling_loop() {
    int64_t last_update_id = 0;
    
    while (running_) {
        std::string params = "offset=" + std::to_string(last_update_id + 1) + 
                            "&timeout=30";
        
        std::string response = make_api_request("getUpdates", params);
        
        // Parse updates (simplified JSON parsing)
        std::regex update_regex(R"("update_id":(\d+))");
        std::smatch match;
        std::string::const_iterator search_start = response.cbegin();
        
        while (std::regex_search(search_start, response.cend(), match, update_regex)) {
            int64_t update_id = std::stoll(match[1].str());
            if (update_id > last_update_id) {
                last_update_id = update_id;
                
                // Find the update JSON block and process it
                size_t start = match.position(0);
                process_update(response.substr(start));
            }
            search_start = match.suffix().first;
        }
        
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.polling_interval_ms)
        );
    }
}

void TelegramBot::process_update(const std::string& update_json) {
    try {
        TelegramMessage msg = parse_message(update_json);
        
        if (msg.text.empty()) return;
        
        // Check if it's a command
        if (msg.text[0] == '/') {
            std::istringstream iss(msg.text.substr(1));
            std::string command;
            iss >> command;
            
            // Remove @botname suffix if present
            size_t at_pos = command.find('@');
            if (at_pos != std::string::npos) {
                command = command.substr(0, at_pos);
            }
            
            // Parse arguments
            std::vector<std::string> args;
            std::string arg;
            while (iss >> arg) {
                args.push_back(arg);
            }
            
            // Also get rest of text as single argument for /query
            size_t space_pos = msg.text.find(' ');
            if (space_pos != std::string::npos) {
                args.insert(args.begin(), msg.text.substr(space_pos + 1));
            }
            
            std::lock_guard lock(commands_mutex_);
            auto it = commands_.find(command);
            
            if (it != commands_.end()) {
                // Check admin permission
                if (it->second.admin_only) {
                    bool is_admin = std::find(
                        config_.admin_user_ids.begin(),
                        config_.admin_user_ids.end(),
                        msg.from.id
                    ) != config_.admin_user_ids.end();
                    
                    if (!is_admin) {
                        send_message(msg.chat_id, "⛔ Access denied. Admin only command.");
                        return;
                    }
                }
                
                std::string response = it->second.handler(msg, args);
                send_message(msg.chat_id, response, true);
            } else if (command == "query" || command == "sql") {
                // Special handling for SQL queries
                if (args.empty()) {
                    send_message(msg.chat_id, "Usage: /query SELECT * FROM users");
                    return;
                }
                
                if (query_executor_) {
                    QueryResult result = query_executor_(args[0]);
                    send_table(msg.chat_id, result);
                } else {
                    send_message(msg.chat_id, "❌ Query executor not configured");
                }
            } else {
                send_message(msg.chat_id, "❓ Unknown command. Try /help");
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to process Telegram update: " << e.what() << "\n";
    }
}

TelegramMessage TelegramBot::parse_message(const std::string& json) {
    TelegramMessage msg{};
    
    // Simplified JSON parsing (in production, use a proper JSON library)
    std::regex chat_id_regex(R"("chat":\s*\{[^}]*"id":\s*(-?\d+))");
    std::regex from_id_regex(R"("from":\s*\{[^}]*"id":\s*(\d+))");
    std::regex username_regex(R"("username":\s*"([^"]*)")");
    std::regex text_regex(R"("text":\s*"([^"]*)")");
    std::regex message_id_regex(R"("message_id":\s*(\d+))");
    
    std::smatch match;
    
    if (std::regex_search(json, match, chat_id_regex)) {
        msg.chat_id = std::stoll(match[1].str());
    }
    
    if (std::regex_search(json, match, from_id_regex)) {
        msg.from.id = std::stoll(match[1].str());
    }
    
    if (std::regex_search(json, match, username_regex)) {
        msg.from.username = match[1].str();
    }
    
    if (std::regex_search(json, match, text_regex)) {
        msg.text = match[1].str();
        // Unescape JSON string
        std::regex unescape_regex(R"(\\n)");
        msg.text = std::regex_replace(msg.text, unescape_regex, "\n");
    }
    
    if (std::regex_search(json, match, message_id_regex)) {
        msg.message_id = std::stoll(match[1].str());
    }
    
    msg.timestamp = std::chrono::system_clock::now();
    
    return msg;
}

std::string TelegramBot::make_api_request(const std::string& method, const std::string& params) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    
    std::string url = api_base_url_ + method;
    if (!params.empty()) {
        url += "?" + params;
    }
    
    std::string response;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "[ERROR] Telegram API request failed: " 
                  << curl_easy_strerror(res) << "\n";
        return "";
    }
    
    return response;
}

} // namespace datyredb::network
