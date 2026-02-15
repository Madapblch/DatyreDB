#include "network/telegram_bot.hpp"

#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <algorithm>

using json = nlohmann::json;

namespace datyredb {
namespace network {

// CURL callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

TelegramBot::TelegramBot(const std::string& token, Database* db)
    : bot_token_(token)
    , db_(db) {
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    init_commands();
    
    if (!test_connection()) {
        throw std::runtime_error("Failed to connect to Telegram API");
    }
}

TelegramBot::~TelegramBot() {
    stop();
    curl_global_cleanup();
}

void TelegramBot::init_commands() {
    commands_["/start"] = [this](const TelegramMessage& msg) {
        std::string welcome = 
            "🚀 *Добро пожаловать в DatyreDB Bot!*\n\n"
            "Я помогу вам управлять базой данных.\n\n"
            "*Доступные команды:*\n"
            "/help - Показать помощь\n"
            "/status - Статус базы данных\n"
            "/tables - Список таблиц\n"
            "/query - Выполнить SQL запрос\n"
            "/stats - Статистика БД\n"
            "/backup - Создать резервную копию\n"
            "/about - О системе";
        
        send_message(msg.chat_id, welcome, true);
    };
    
    commands_["/help"] = [this](const TelegramMessage& msg) {
        std::string help_text = 
            "📋 *Справка по командам:*\n\n"
            "*Основные команды:*\n"
            "`/query SELECT * FROM table` - выполнить запрос\n"
            "`/tables` - показать все таблицы\n\n"
            "*Администрирование:*\n"
            "`/status` - состояние БД\n"
            "`/stats` - подробная статистика\n"
            "`/backup` - создать бэкап\n\n"
            "*Примеры SQL:*\n"
            "`/query SELECT * FROM users`\n"
            "`/query SHOW TABLES`\n"
            "`/query INSERT INTO users VALUES ('4', 'Dan', 'dan@test.com', '2024-01-04')`";
        
        send_message(msg.chat_id, help_text, true);
    };
    
    commands_["/status"] = [this](const TelegramMessage& msg) {
        if (!db_) {
            send_message(msg.chat_id, "❌ База данных не подключена");
            return;
        }
        
        auto stats = db_->get_statistics();
        std::ostringstream ss;
        ss << "📊 *Статус базы данных:*\n\n"
           << "✅ Состояние: Активна\n"
           << "📁 Таблиц: " << stats.table_count << "\n"
           << "📝 Записей: " << stats.total_records << "\n"
           << "💾 Размер: " << format_bytes(stats.total_size) << "\n"
           << "⚡ Индексов: " << stats.index_count << "\n"
           << "🕐 Uptime: " << format_duration(stats.uptime_seconds);
        
        send_message(msg.chat_id, ss.str(), true);
    };
    
    commands_["/tables"] = [this](const TelegramMessage& msg) {
        if (!db_) {
            send_message(msg.chat_id, "❌ База данных не подключена");
            return;
        }
        
        auto tables = db_->list_tables();
        if (tables.empty()) {
            send_message(msg.chat_id, "📭 Нет таблиц в базе данных");
            return;
        }
        
        std::ostringstream ss;
        ss << "📋 *Список таблиц:*\n\n";
        
        for (const auto& table : tables) {
            auto table_stats = db_->get_table_statistics(table);
            ss << "• `" << table << "` - " 
               << table_stats.record_count << " записей, "
               << format_bytes(table_stats.size_bytes) << "\n";
        }
        
        send_message(msg.chat_id, ss.str(), true);
    };
    
    commands_["/query"] = [this](const TelegramMessage& msg) {
        if (!db_) {
            send_message(msg.chat_id, "❌ База данных не подключена");
            return;
        }
        
        // Extract SQL query
        std::string query;
        if (msg.text.length() > 7) {
            query = msg.text.substr(7);
            // Trim leading spaces
            size_t start = query.find_first_not_of(" \t");
            if (start != std::string::npos) {
                query = query.substr(start);
            }
        }
        
        if (query.empty()) {
            send_message(msg.chat_id, 
                "❌ Использование: `/query SQL_ЗАПРОС`\n"
                "Пример: `/query SELECT * FROM users`", true);
            return;
        }
        
        try {
            auto start = std::chrono::high_resolution_clock::now();
            auto result = db_->execute_query(query);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::ostringstream ss;
            
            if (result.success) {
                ss << "✅ *Запрос выполнен успешно*\n"
                   << "⏱ Время: " << duration.count() << " мс\n\n";
                
                if (result.has_data()) {
                    ss << "```\n" << format_query_result(result) << "```";
                } else {
                    ss << "Затронуто строк: " << result.affected_rows;
                }
            } else {
                ss << "❌ *Ошибка:* " << result.error_message;
            }
            
            send_message(msg.chat_id, ss.str(), true);
            
        } catch (const std::exception& e) {
            send_message(msg.chat_id, 
                "❌ *Ошибка выполнения запроса:*\n`" + std::string(e.what()) + "`", true);
        }
    };
    
    commands_["/stats"] = [this](const TelegramMessage& msg) {
        if (!db_) {
            send_message(msg.chat_id, "❌ База данных не подключена");
            return;
        }
        
        auto stats = db_->get_detailed_statistics();
        std::ostringstream ss;
        
        ss << "📊 *Детальная статистика БД:*\n\n"
           << "*Общая информация:*\n"
           << "• Версия: " << stats.version << "\n"
           << "• Uptime: " << format_duration(stats.uptime_seconds) << "\n\n"
           << "*Использование ресурсов:*\n"
           << "• RAM: " << format_bytes(stats.memory_used) << " / " 
           << format_bytes(stats.memory_total) << "\n"
           << "• Диск: " << format_bytes(stats.disk_used) << " / "
           << format_bytes(stats.disk_total) << "\n"
           << "• CPU: " << stats.cpu_usage << "%\n\n"
           << "*Производительность:*\n"
           << "• Запросов/сек: " << stats.queries_per_second << "\n"
           << "• Среднее время: " << stats.avg_query_time << " мс\n"
           << "• Соединений: " << stats.active_connections << "\n"
           << "• Кэш хитов: " << stats.cache_hit_ratio << "%";
        
        send_message(msg.chat_id, ss.str(), true);
    };
    
    commands_["/backup"] = [this](const TelegramMessage& msg) {
        if (!db_) {
            send_message(msg.chat_id, "❌ База данных не подключена");
            return;
        }
        
        send_message(msg.chat_id, "🔄 Создание резервной копии...");
        
        try {
            std::string backup_path = db_->create_backup();
            
            if (backup_path.empty()) {
                send_message(msg.chat_id, "❌ Не удалось создать резервную копию");
                return;
            }
            
            std::ostringstream ss;
            ss << "✅ *Резервная копия создана!*\n\n"
               << "📁 Файл: `" << backup_path << "`\n"
               << "📏 Размер: " << format_bytes(get_file_size(backup_path)) << "\n"
               << "🕐 Время: " << get_current_timestamp();
            
            send_message(msg.chat_id, ss.str(), true);
            
        } catch (const std::exception& e) {
            send_message(msg.chat_id, 
                "❌ *Ошибка создания бэкапа:*\n`" + std::string(e.what()) + "`", true);
        }
    };
    
    commands_["/about"] = [this](const TelegramMessage& msg) {
        std::string about = 
            "🔷 *DatyreDB v1.0.0*\n\n"
            "Высокопроизводительная СУБД нового поколения\n\n"
            "*Возможности:*\n"
            "• Multi-threading обработка запросов\n"
            "• B-Tree индексирование\n"
            "• ACID транзакции\n"
            "• REST API и Telegram интеграция\n\n"
            "*Разработчик:* @madapblch\n"
            "*GitHub:* github.com/Madapblch/DatyreDB";
        
        send_message(msg.chat_id, about, true);
    };
}

bool TelegramBot::test_connection() {
    try {
        std::string response = make_request("getMe");
        auto j = json::parse(response);
        
        if (j["ok"].get<bool>()) {
            auto result = j["result"];
            std::cout << "[TelegramBot] Connected as @" 
                      << result["username"].get<std::string>() 
                      << std::endl;
            return true;
        }
        
        return false;
    } catch (...) {
        return false;
    }
}

std::string TelegramBot::make_request(const std::string& method, const std::string& params) {
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    std::string url = "https://api.telegram.org/bot" + bot_token_ + "/" + method;
    if (!params.empty()) {
        url += "?" + params;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw std::runtime_error(curl_easy_strerror(res));
    }
    
    return response;
}

TelegramMessage TelegramBot::parse_message(const std::string& json_str) {
    TelegramMessage message;
    
    try {
        auto j = json::parse(json_str);
        
        json msg_object;
        if (j.contains("message")) {
            msg_object = j["message"];
        } else if (j.contains("callback_query")) {
            msg_object = j["callback_query"];
        } else {
            return message;
        }
        
        if (msg_object.contains("chat") && msg_object["chat"].contains("id")) {
            message.chat_id = msg_object["chat"]["id"].get<int64_t>();
        }
        
        if (msg_object.contains("from")) {
            auto& from = msg_object["from"];
            if (from.contains("id")) {
                message.from_id = from["id"].get<int64_t>();
            }
            if (from.contains("username")) {
                message.username = from["username"].get<std::string>();
            }
        }
        
        if (msg_object.contains("text")) {
            message.text = msg_object["text"].get<std::string>();
        }
        
        if (msg_object.contains("message_id")) {
            message.message_id = msg_object["message_id"].get<int64_t>();
        }
        
    } catch (...) {
        // Ignore parse errors
    }
    
    return message;
}

void TelegramBot::send_message(int64_t chat_id, const std::string& text, bool parse_markdown) {
    CURL* curl = curl_easy_init();
    if (!curl) return;
    
    std::string response;
    std::string url = "https://api.telegram.org/bot" + bot_token_ + "/sendMessage";
    
    json post_data;
    post_data["chat_id"] = chat_id;
    post_data["text"] = text;
    
    if (parse_markdown) {
        post_data["parse_mode"] = "Markdown";
    }
    
    std::string post_fields = post_data.dump();
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

void TelegramBot::process_updates() {
    std::ostringstream params;
    params << "offset=" << update_offset_ << "&timeout=30";
    
    try {
        std::string response = make_request("getUpdates", params.str());
        auto j = json::parse(response);
        
        if (!j["ok"].get<bool>()) return;
        
        for (const auto& update : j["result"]) {
            update_offset_ = update["update_id"].get<int>() + 1;
            
            TelegramMessage msg = parse_message(update.dump());
            
            if (!msg.text.empty()) {
                handle_message(msg);
            }
        }
    } catch (...) {
        // Ignore errors
    }
}

void TelegramBot::handle_message(const TelegramMessage& msg) {
    if (msg.text.empty() || msg.text[0] != '/') {
        send_message(msg.chat_id, "💡 Используйте /help для справки");
        return;
    }
    
    // Extract command
    std::string command = msg.text;
    size_t space_pos = command.find(' ');
    if (space_pos != std::string::npos) {
        command = command.substr(0, space_pos);
    }
    
    // Convert to lowercase for comparison
    std::transform(command.begin(), command.end(), command.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    auto it = commands_.find(command);
    if (it != commands_.end()) {
        try {
            it->second(msg);
        } catch (const std::exception& e) {
            send_message(msg.chat_id, "❌ Ошибка: " + std::string(e.what()));
        }
    } else if (command.find("/query") == 0) {
        commands_["/query"](msg);
    } else {
        send_message(msg.chat_id, "❓ Неизвестная команда. /help для справки.");
    }
}

void TelegramBot::start() {
    if (running_) return;
    
    running_ = true;
    bot_thread_ = std::thread(&TelegramBot::bot_loop, this);
    
    std::cout << "[TelegramBot] Bot started" << std::endl;
}

void TelegramBot::stop() {
    if (!running_) return;
    
    running_ = false;
    
    if (bot_thread_.joinable()) {
        bot_thread_.join();
    }
    
    std::cout << "[TelegramBot] Bot stopped" << std::endl;
}

void TelegramBot::bot_loop() {
    while (running_) {
        try {
            process_updates();
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

// Static helper methods
std::string TelegramBot::format_bytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024 && unit_index < 4) {
        size /= 1024;
        unit_index++;
    }
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return ss.str();
}

std::string TelegramBot::format_duration(int seconds) {
    int days = seconds / 86400;
    int hours = (seconds % 86400) / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    std::ostringstream ss;
    if (days > 0) ss << days << "д ";
    if (hours > 0) ss << hours << "ч ";
    if (minutes > 0) ss << minutes << "м ";
    ss << secs << "с";
    
    return ss.str();
}

// ИСПРАВЛЕНО: result.columns и result.rows - это поля, не методы
std::string TelegramBot::format_query_result(const QueryResult& result) {
    std::ostringstream ss;
    
    // Headers
    for (const auto& col : result.columns) {  // БЕЗ скобок!
        ss << col << " | ";
    }
    ss << "\n";
    
    // Separator
    for (size_t i = 0; i < result.columns.size(); i++) {  // БЕЗ скобок!
        ss << "--- | ";
    }
    ss << "\n";
    
    // Data
    size_t count = 0;
    for (const auto& row : result.rows) {  // БЕЗ скобок!
        if (count++ >= 10) {
            ss << "\n... и ещё " << (result.rows.size() - 10) << " строк";  // БЕЗ скобок!
            break;
        }
        for (const auto& val : row) {
            ss << val << " | ";
        }
        ss << "\n";
    }
    
    return ss.str();
}

std::string TelegramBot::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

size_t TelegramBot::get_file_size(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    return file.good() ? static_cast<size_t>(file.tellg()) : 0;
}

} // namespace network
} // namespace datyredb
