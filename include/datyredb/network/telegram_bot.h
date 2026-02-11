#pragma once

#include "types.h"
#include "http_server.h"
#include <functional>
#include <queue>
#include <thread>

namespace datyredb::network {

struct TelegramUser {
    int64_t id;
    std::string username;
    std::string first_name;
    std::string last_name;
    bool is_admin{false};
};

struct TelegramMessage {
    int64_t message_id;
    int64_t chat_id;
    TelegramUser from;
    std::string text;
    std::chrono::system_clock::time_point timestamp;
};

struct TelegramConfig {
    std::string bot_token;
    std::string webhook_url;            // For webhook mode
    bool use_webhook{false};            // false = polling mode
    std::vector<int64_t> admin_user_ids; // Users allowed to run admin commands
    size_t polling_interval_ms{1000};
};

class TelegramBot {
public:
    explicit TelegramBot(const TelegramConfig& config);
    ~TelegramBot();
    
    // Lifecycle
    bool start();
    void stop();
    
    // Sending messages
    bool send_message(int64_t chat_id, const std::string& text, bool markdown = false);
    bool send_code_block(int64_t chat_id, const std::string& code, const std::string& language = "sql");
    bool send_table(int64_t chat_id, const QueryResult& result);
    
    // Command handlers
    using CommandHandler = std::function<std::string(const TelegramMessage&, const std::vector<std::string>&)>;
    void register_command(const std::string& command, CommandHandler handler, bool admin_only = false);
    
    // Query execution callback
    using QueryExecutor = std::function<QueryResult(const std::string&)>;
    void set_query_executor(QueryExecutor executor) { query_executor_ = std::move(executor); }
    
    // Webhook handler (for HTTP server integration)
    HttpResponse handle_webhook(const HttpRequest& req);
    
private:
    void polling_loop();
    void process_update(const std::string& update_json);
    TelegramMessage parse_message(const std::string& json);
    std::string make_api_request(const std::string& method, const std::string& params = "");
    std::string escape_markdown(const std::string& text);
    
    TelegramConfig config_;
    std::atomic<bool> running_{false};
    std::thread polling_thread_;
    
    // Command registry
    struct CommandInfo {
        CommandHandler handler;
        bool admin_only;
    };
    std::map<std::string, CommandInfo> commands_;
    mutable std::mutex commands_mutex_;
    
    QueryExecutor query_executor_;
    
    // HTTP client for Telegram API
    std::string api_base_url_;
};

} // namespace datyredb::network
