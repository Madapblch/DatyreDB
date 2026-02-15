#pragma once

#include "core/database.hpp"

#include <string>
#include <functional>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <memory>

namespace datyredb {
namespace network {

struct TelegramMessage {
    int64_t chat_id = 0;
    int64_t from_id = 0;
    int64_t message_id = 0;
    std::string username;
    std::string text;
};

class TelegramBot {
public:
    TelegramBot(const std::string& token, Database* db = nullptr);
    ~TelegramBot();
    
    // Lifecycle
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Messaging
    void send_message(int64_t chat_id, const std::string& text, bool parse_markdown = false);
    
private:
    using CommandHandler = std::function<void(const TelegramMessage&)>;
    
    // Core functionality
    void init_commands();
    void bot_loop();
    void process_updates();
    void handle_message(const TelegramMessage& msg);
    
    // API methods
    bool test_connection();
    std::string make_request(const std::string& method, const std::string& params = "");
    TelegramMessage parse_message(const std::string& json);
    
    // Helper methods
    static std::string format_bytes(size_t bytes);
    static std::string format_duration(int seconds);
    std::string format_query_result(const QueryResult& result);
    static std::string get_current_timestamp();
    static size_t get_file_size(const std::string& path);
    
    // Member variables
    std::string bot_token_;
    Database* db_;
    
    std::unordered_map<std::string, CommandHandler> commands_;
    
    std::atomic<bool> running_{false};
    std::thread bot_thread_;
    int update_offset_ = 0;
};

} // namespace network
} // namespace datyredb
