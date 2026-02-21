#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>

// ВАЖНО: Подключаем определение типа QueryResult, так как он используется по значению/ссылке
#include "core/query_result.hpp"

namespace datyre {
    // Forward declaration
    class Database;

namespace network {

    class TelegramBot {
    public:
        // Используем указатель или ссылку на Database
        TelegramBot(const std::string& token, datyre::Database* db = nullptr);
        ~TelegramBot();

        void start();
        void stop();

    private:
        std::string token_;
        // Указатель на базу данных (не владеющий, observer pointer)
        datyre::Database* db_;
        
        std::atomic<bool> running_{false};
        std::thread poll_thread_;
        long long last_update_id_ = 0;

        void poll_updates();
        void handle_message(long long chat_id, const std::string& text);
        void send_message(long long chat_id, const std::string& text);
        
        // Вспомогательный метод форматирования
        std::string format_query_result(const QueryResult& result);
    };

} // namespace network
} // namespace datyre
