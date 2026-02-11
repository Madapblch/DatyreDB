#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <optional>
#include <atomic>
#include <thread>

namespace datyredb::network {

// ===== Network Configuration =====
struct ServerConfig {
    // TCP Server
    std::string bind_address{"0.0.0.0"};
    uint16_t tcp_port{5432};
    
    // HTTP/REST API
    bool enable_http{true};
    uint16_t http_port{8080};
    
    // WebSocket
    bool enable_websocket{true};
    uint16_t websocket_port{8081};
    
    // Telegram
    bool enable_telegram{false};
    std::string telegram_bot_token;
    std::string telegram_webhook_url;
    std::vector<int64_t> telegram_admin_ids;
    
    // SSL/TLS
    bool enable_ssl{false};
    std::string ssl_cert_path;
    std::string ssl_key_path;
    
    // Connection limits
    size_t max_connections{1000};
    size_t max_connections_per_ip{100};
    std::chrono::seconds connection_timeout{300};
    std::chrono::seconds idle_timeout{60};
    
    // Thread pool
    size_t worker_threads{4};
    size_t io_threads{2};
    
    // Buffer sizes
    size_t read_buffer_size{65536};
    size_t write_buffer_size{65536};
};

// ===== Message Types =====
enum class MessageType : uint8_t {
    // Authentication
    AUTH_REQUEST = 0x01,
    AUTH_RESPONSE = 0x02,
    AUTH_OK = 0x03,
    AUTH_FAILED = 0x04,
    
    // Query
    QUERY = 0x10,
    QUERY_RESULT = 0x11,
    ROW_DATA = 0x12,
    COMMAND_COMPLETE = 0x13,
    
    // Errors
    ERROR = 0x20,
    WARNING = 0x21,
    NOTICE = 0x22,
    
    // Control
    PING = 0x30,
    PONG = 0x31,
    TERMINATE = 0x32,
    READY = 0x33,
    
    // Transaction
    BEGIN_TXN = 0x40,
    COMMIT_TXN = 0x41,
    ROLLBACK_TXN = 0x42,
    TXN_STATUS = 0x43
};

// ===== Connection State =====
enum class ConnectionState {
    CONNECTING,
    AUTHENTICATING,
    READY,
    EXECUTING,
    CLOSING,
    CLOSED
};

// ===== Error Codes =====
enum class ErrorCode : uint32_t {
    OK = 0,
    UNKNOWN_ERROR = 1,
    CONNECTION_REFUSED = 2,
    AUTH_FAILED = 3,
    QUERY_ERROR = 4,
    TIMEOUT = 5,
    PROTOCOL_ERROR = 6,
    INTERNAL_ERROR = 7,
    NOT_FOUND = 8,
    PERMISSION_DENIED = 9
};

// ===== Query Result =====
struct QueryResult {
    bool success{false};
    ErrorCode error_code{ErrorCode::OK};
    std::string error_message;
    
    std::vector<std::string> column_names;
    std::vector<std::vector<std::string>> rows;
    
    size_t rows_affected{0};
    std::chrono::microseconds execution_time{0};
    
    std::string to_json() const;
    std::string to_table_string() const;
};

} // namespace datyredb::network
