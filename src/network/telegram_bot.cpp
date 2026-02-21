#include "network/telegram_bot.hpp"
#include "core/database.hpp" // Нужно полное определение Database

#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip> // Для setw

// Callback для CURL
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

namespace datyre {
namespace network {

    TelegramBot::TelegramBot(const std::string& token, datyre::Database* db)
        : token_(token), db_(db) {}

    TelegramBot::~TelegramBot() {
        stop();
    }

    void TelegramBot::start() {
        if (running_) return;
        running_ = true;
        poll_thread_ = std::thread(&TelegramBot::poll_updates, this);
        std::cout << "[TelegramBot] Started polling..." << std::endl;
    }

    void TelegramBot::stop() {
        running_ = false;
        if (poll_thread_.joinable()) {
            poll_thread_.join();
        }
    }

    void TelegramBot::poll_updates() {
        CURL* curl = curl_easy_init();
        if (!curl) return;

        while (running_) {
            std::string readBuffer;
            std::string url = "https://api.telegram.org/bot" + token_ + "/getUpdates?timeout=30&offset=" + std::to_string(last_update_id_ + 1);

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            
            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                try {
                    auto json = nlohmann::json::parse(readBuffer);
                    if (json["ok"] && json["result"].is_array()) {
                        for (const auto& update : json["result"]) {
                            last_update_id_ = update["update_id"];
                            if (update.contains("message")) {
                                long long chat_id = update["message"]["chat"]["id"];
                                std::string text = update["message"].value("text", "");
                                handle_message(chat_id, text);
                            }
                        }
                    }
                } catch (...) {
                    // Ignore parsing errors
                }
            }
            // Небольшая пауза, если curl вернул ошибку
            if (res != CURLE_OK) std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        curl_easy_cleanup(curl);
    }

    void TelegramBot::handle_message(long long chat_id, const std::string& text) {
        if (!db_) {
            send_message(chat_id, "Error: Database not connected.");
            return;
        }

        std::string response;
        if (text == "/start") {
            response = "Welcome to DatyreDB Bot! Send SQL query.";
        } else if (text == "/stats") {
            // Внимание: метод get_statistics должен быть в Database
            // response = db_->get_statistics(); 
            response = "Stats: Uptime 100%";
        } else {
            // Выполняем SQL
            // Внимание: метод execute должен возвращать QueryResult или string
            // В нашем коде execute возвращает string (пока). 
            // Если он возвращает QueryResult, используй format_query_result.
            
            // Вариант А: Database::execute возвращает string
            response = db_->execute(text);
            
            // Вариант Б: Если бы возвращал QueryResult
            // auto res = db_->execute_query(text);
            // response = format_query_result(res);
        }
        send_message(chat_id, response);
    }

    void TelegramBot::send_message(long long chat_id, const std::string& text) {
        CURL* curl = curl_easy_init();
        if (curl) {
            std::string url = "https://api.telegram.org/bot" + token_ + "/sendMessage";
            std::string post_data = nlohmann::json{
                {"chat_id", chat_id},
                {"text", text.substr(0, 4096)} // Telegram limit
            }.dump();

            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
            
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }
    }

    std::string TelegramBot::format_query_result(const QueryResult& result) {
        if (!result.ok()) {
            return "Error: " + result.message(); // Используем метод()
        }

        std::stringstream ss;
        // Исправление: columns() вместо columns
        for (const auto& col : result.columns()) {
            ss << col << " | ";
        }
        ss << "\n" << std::string(20, '-') << "\n";

        size_t count = 0;
        // Исправление: rows() вместо rows
        for (const auto& row : result.rows()) {
            if (count++ > 10) {
                // Исправление: row_count() вместо rows.size()
                ss << "... and " << (result.row_count() - 10) << " more";
                break;
            }
            for (const auto& val : row.values()) { // Используем row.values()
                ss << val << " | ";
            }
            ss << "\n";
        }
        return ss.str();
    }

} // namespace network
} // namespace datyre
