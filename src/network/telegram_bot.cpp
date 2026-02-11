#include "telegram_bot.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <regex>

using json = nlohmann::json;

namespace datyredb {
namespace network {

// Callback для CURL - запись ответа в строку
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

TelegramBot::TelegramBot(const std::string& token, database::Database* db)
    : bot_token_(token)
    , db_(db)
    , running_(false)
    , update_offset_(0) {
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Инициализация команд
    init_commands();
    
    // Проверка соединения
    if (!test_connection()) {
        throw std::runtime_error("Failed to connect to Telegram API");
    }
}

TelegramBot::~TelegramBot() {
    stop();
    curl_global_cleanup();
}

void TelegramBot::init_commands() {
    // Базовые команды
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
            "`/tables` - показать все таблицы\n"
            "`/describe table_name` - структура таблицы\n\n"
            "*Управление данными:*\n"
            "`/insert table_name` - добавить данные\n"
            "`/update table_name` - обновить данные\n"
            "`/delete table_name` - удалить данные\n\n"
            "*Администрирование:*\n"
            "`/status` - состояние БД\n"
            "`/stats` - подробная статистика\n"
            "`/backup` - создать бэкап\n"
            "`/restore` - восстановить из бэкапа\n"
            "`/optimize` - оптимизация БД\n\n"
            "*Примеры:*\n"
            "`/query CREATE TABLE users (id INT, name TEXT)`\n"
            "`/query INSERT INTO users VALUES (1, 'Alice')`\n"
            "`/query SELECT * FROM users WHERE id = 1`";
        
        send_message(msg.chat_id, help_text, true);
    };
    
    commands_["/status"] = [this](const TelegramMessage& msg) {
        if (!db_) {
            send_message(msg.chat_id, "❌ База данных не подключена");
            return;
        }
        
        auto stats = db_->get_statistics();
        std::stringstream ss;
        ss << "📊 *Статус базы данных:*\n\n";
        ss << "✅ Состояние: Активна\n";
        ss << "📁 Таблиц: " << stats.table_count << "\n";
        ss << "📝 Записей: " << stats.total_records << "\n";
        ss << "💾 Размер: " << format_bytes(stats.total_size) << "\n";
        ss << "⚡ Индексов: " << stats.index_count << "\n";
        ss << "🕐 Uptime: " << format_duration(stats.uptime_seconds);
        
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
        
        std::stringstream ss;
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
        
        // Извлекаем SQL запрос из сообщения
        std::string query = msg.text.substr(6); // Убираем "/query "
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
            
            std::stringstream ss;
            ss << "✅ *Запрос выполнен успешно*\n";
            ss << "⏱ Время: " << duration.count() << " мс\n\n";
            
            if (result.has_data()) {
                ss << "```\n" << format_query_result(result) << "\n```";
            } else {
                ss << "Затронуто строк: " << result.affected_rows();
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
        std::stringstream ss;
        
        ss << "📊 *Детальная статистика БД:*\n\n";
        ss << "*Общая информация:*\n";
        ss << "• Версия: " << stats.version << "\n";
        ss << "• Uptime: " << format_duration(stats.uptime_seconds) << "\n\n";
        
        ss << "*Использование ресурсов:*\n";
        ss << "• RAM: " << format_bytes(stats.memory_used) << " / " 
           << format_bytes(stats.memory_total) << "\n";
        ss << "• Диск: " << format_bytes(stats.disk_used) << " / "
           << format_bytes(stats.disk_total) << "\n";
        ss << "• CPU: " << stats.cpu_usage << "%\n\n";
        
        ss << "*Производительность:*\n";
        ss << "• Запросов/сек: " << stats.queries_per_second << "\n";
        ss << "• Среднее время отклика: " << stats.avg_query_time << " мс\n";
        ss << "• Активных соединений: " << stats.active_connections << "\n";
        ss << "• Кэш хитов: " << stats.cache_hit_ratio << "%\n";
        
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
            
            std::stringstream ss;
            ss << "✅ *Резервная копия создана успешно!*\n\n";
            ss << "📁 Файл: `" << backup_path << "`\n";
            ss << "📏 Размер: " << format_bytes(get_file_size(backup_path)) << "\n";
            ss << "🕐 Время создания: " << get_current_timestamp();
            
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
            "• Репликация Master-Slave\n"
            "• Автоматическое резервное копирование\n"
            "• REST API и Telegram интеграция\n\n"
            "*Разработчик:* @madapblch\n"
            "*GitHub:* [DatyreDB](https://github.com/Madapblch/DatyreDB)";
        
        send_message(msg.chat_id, about, true);
    };
}

bool TelegramBot::test_connection() {
    try {
        std::string response = make_request("getMe");
        auto j = json::parse(response);
        
        if (j["ok"].get<bool>()) {
            auto result = j["result"];
            std::cout << "[TelegramBot] Connected as @" << result["username"].get<std::string>() 
                      << " (ID: " << result["id"].get<int64_t>() << ")" << std::endl;
            return true;
        }
        
        std::cerr << "[TelegramBot] Connection failed: " << j["description"].get<std::string>() << std::endl;
        return false;
        
    } catch (const std::exception& e) {
        std::cerr << "[TelegramBot] Connection test failed: " << e.what() << std::endl;
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
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }
    
    return response;
}

TelegramMessage TelegramBot::parse_message(const std::string& json_str) {
    TelegramMessage message;
    
    if (json_str.empty()) {
        return message;
    }
    
    try {
        auto j = json::parse(json_str);
        
        // Проверяем тип update
        json msg_object;
        bool is_callback = false;
        
        if (j.contains("message")) {
            msg_object = j["message"];
        } else if (j.contains("callback_query")) {
            msg_object = j["callback_query"];
            is_callback = true;
        } else if (j.contains("edited_message")) {
            msg_object = j["edited_message"];
        } else {
            return message;
        }
        
        // Извлекаем chat_id
        if (is_callback && msg_object.contains("message")) {
            if (msg_object["message"].contains("chat") && 
                msg_object["message"]["chat"].contains("id")) {
                message.chat_id = msg_object["message"]["chat"]["id"].get<int64_t>();
            }
        } else if (msg_object.contains("chat") && msg_object["chat"].contains("id")) {
            message.chat_id = msg_object["chat"]["id"].get<int64_t>();
        }
        
        // Извлекаем from_id и username
        if (msg_object.contains("from")) {
            auto& from = msg_object["from"];
            if (from.contains("id")) {
                message.from_id = from["id"].get<int64_t>();
            }
            if (from.contains("username")) {
                message.username = from["username"].get<std::string>();
            } else if (from.contains("first_name")) {
                message.username = from["first_name"].get<std::string>();
            }
        }
        
        // Извлекаем текст
        if (is_callback && msg_object.contains("data")) {
            message.text = msg_object["data"].get<std::string>();
        } else if (msg_object.contains("text")) {
            message.text = msg_object["text"].get<std::string>();
        }
        
        // Message ID
        if (!is_callback && msg_object.contains("message_id")) {
            message.message_id = msg_object["message_id"].get<int64_t>();
        }
        
    } catch (const json::exception& e) {
        std::cerr << "[TelegramBot] JSON parsing error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[TelegramBot] Unexpected error in parse_message: " << e.what() << std::endl;
    }
    
    return message;
}

void TelegramBot::send_message(int64_t chat_id, const std::string& text, bool parse_markdown) {
    CURL* curl = curl_easy_init();
    if (!curl) return;
    
    std::string response;
    std::string url = "https://api.telegram.org/bot" + bot_token_ + "/sendMessage";
    
    // Подготовка POST данных
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
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "[TelegramBot] Failed to send message: " << curl_easy_strerror(res) << std::endl;
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

void TelegramBot::process_updates() {
    std::stringstream params;
    params << "offset=" << update_offset_ << "&timeout=30";
    
    try {
        std::string response = make_request("getUpdates", params.str());
        auto j = json::parse(response);
        
        if (!j["ok"].get<bool>()) {
            std::cerr << "[TelegramBot] Failed to get updates: " 
                      << j["description"].get<std::string>() << std::endl;
            return;
        }
        
        auto updates = j["result"];
        
        for (const auto& update : updates) {
            // Обновляем offset
            int update_id = update["update_id"].get<int>();
            update_offset_ = update_id + 1;
            
            // Парсим сообщение
            TelegramMessage msg = parse_message(update.dump());
            
            if (!msg.text.empty()) {
                std::cout << "[TelegramBot] Message from " << msg.username 
                          << " (" << msg.from_id << "): " << msg.text << std::endl;
                
                // Обрабатываем сообщение
                handle_message(msg);
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[TelegramBot] Error processing updates: " << e.what() << std::endl;
    }
}

void TelegramBot::handle_message(const TelegramMessage& msg) {
    // Проверяем, является ли это командой
    if (msg.text[0] == '/') {
        size_t space_pos = msg.text.find(' ');
        std::string command = (space_pos != std::string::npos) 
            ? msg.text.substr(0, space_pos) 
            : msg.text;
        
        // Ищем обработчик команды
        auto it = commands_.find(command);
        if (it != commands_.end()) {
            try {
                it->second(msg);
            } catch (const std::exception& e) {
                send_message(msg.chat_id, 
                    "❌ Ошибка выполнения команды: " + std::string(e.what()));
            }
        } else if (command.find("/query") == 0) {
            // Специальная обработка для /query с параметрами
            commands_["/query"](msg);
        } else {
            send_message(msg.chat_id, 
                "❓ Неизвестная команда. Используйте /help для справки.");
        }
    } else {
        // Не команда - можно обработать как обычный текст или SQL запрос
        send_message(msg.chat_id, 
            "💡 Подсказка: используйте /help чтобы увидеть доступные команды");
    }
}

void TelegramBot::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    bot_thread_ = std::thread(&TelegramBot::bot_loop, this);
    
    std::cout << "[TelegramBot] Bot started successfully" << std::endl;
}

void TelegramBot::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (bot_thread_.joinable()) {
        bot_thread_.join();
    }
    
    std::cout << "[TelegramBot] Bot stopped" << std::endl;
}

void TelegramBot::bot_loop() {
    std::cout << "[TelegramBot] Entering bot loop..." << std::endl;
    
    while (running_) {
        try {
            process_updates();
        } catch (const std::exception& e) {
            std::cerr << "[TelegramBot] Error in bot loop: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

// Вспомогательные функции форматирования
std::string TelegramBot::format_bytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024 && unit_index < 4) {
        size /= 1024;
        unit_index++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return ss.str();
}

std::string TelegramBot::format_duration(int seconds) {
    int days = seconds / 86400;
    int hours = (seconds % 86400) / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    std::stringstream ss;
    if (days > 0) ss << days << "д ";
    if (hours > 0) ss << hours << "ч ";
    if (minutes > 0) ss << minutes << "м ";
    ss << secs << "с";
    
    return ss.str();
}

std::string TelegramBot::format_query_result(const database::QueryResult& result) {
    // Простое форматирование результата запроса
    std::stringstream ss;
    
    // Заголовки колонок
    for (const auto& col : result.columns()) {
        ss << col << " | ";
    }
    ss << "\n";
    
    // Строка разделитель
    for (size_t i = 0; i < result.columns().size(); i++) {
        ss << "--- | ";
    }
    ss << "\n";
    
    // Данные
    size_t row_count = 0;
    const size_t max_rows = 10; // Ограничение для Telegram
    
    for (const auto& row : result.rows()) {
        if (row_count++ >= max_rows) {
            ss << "\n... и ещё " << (result.rows().size() - max_rows) << " строк";
            break;
        }
        
        for (const auto& value : row) {
            ss << value << " | ";
        }
        ss << "\n";
    }
    
    return ss.str();
}

std::string TelegramBot::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

size_t TelegramBot::get_file_size(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    return file.good() ? static_cast<size_t>(file.tellg()) : 0;
}

} // namespace network
} // namespace datyredb
