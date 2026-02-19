#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <fstream>  // Для работы с файлами
#include <iostream>
#include "table.hpp"

namespace datyre {

    const std::string WAL_FILE = "datyre.wal"; // Имя файла журнала

    class DatabaseEngine {
    private:
        std::map<std::string, Table> tables;
        std::mutex db_mutex;
        std::ofstream wal_writer; // Поток для записи в журнал

        // Разделение строки
        std::vector<std::string> split(const std::string& str, char delimiter) {
            std::vector<std::string> tokens;
            std::string token;
            std::istringstream tokenStream(str);
            while (std::getline(tokenStream, token, delimiter)) {
                token.erase(0, token.find_first_not_of(" \t\n\r"));
                token.erase(token.find_last_not_of(" \t\n\r") + 1);
                if (!token.empty()) {
                    tokens.push_back(token);
                }
            }
            return tokens;
        }

        std::string get_first_word(const std::string& str) {
            std::stringstream ss(str);
            std::string word;
            ss >> word;
            std::transform(word.begin(), word.end(), word.begin(), ::toupper);
            return word;
        }

        // --- Внутренние методы логики (не пишут на диск сами) ---

        void internal_create(const std::string& query) {
            size_t open_paren = query.find('(');
            size_t close_paren = query.find(')');
            if (open_paren == std::string::npos || close_paren == std::string::npos) throw std::runtime_error("Syntax error");

            std::string pre_paren = query.substr(0, open_paren);
            std::stringstream ss(pre_paren);
            std::string temp, table_name;
            ss >> temp >> temp >> table_name;

            if (tables.find(table_name) != tables.end()) throw std::runtime_error("Table already exists");

            std::string cols_str = query.substr(open_paren + 1, close_paren - open_paren - 1);
            std::vector<std::string> columns = split(cols_str, ',');
            tables[table_name] = Table(table_name, columns);
        }

        void internal_insert(const std::string& query) {
            size_t open_paren = query.find('(');
            size_t close_paren = query.find(')');
            if (open_paren == std::string::npos || close_paren == std::string::npos) throw std::runtime_error("Syntax error");

            std::string pre_paren = query.substr(0, open_paren);
            std::stringstream ss(pre_paren);
            std::string temp, table_name;
            ss >> temp >> temp >> table_name; 

            if (tables.find(table_name) == tables.end()) throw std::runtime_error("Table not found");

            std::string vals_str = query.substr(open_paren + 1, close_paren - open_paren - 1);
            std::vector<std::string> values = split(vals_str, ',');
            tables[table_name].insert(values);
        }

        std::string internal_select(const std::string& query) {
            std::stringstream ss(query);
            std::string temp, table_name;
            ss >> temp >> temp >> temp >> table_name;

            if (tables.find(table_name) == tables.end()) throw std::runtime_error("Table not found");
            return tables[table_name].to_string();
        }

        // --- Механизм восстановления (Recovery) ---
        void recover() {
            std::ifstream wal_reader(WAL_FILE);
            if (!wal_reader.is_open()) return; // Файла нет, значит база новая

            std::string line;
            int count = 0;
            while (std::getline(wal_reader, line)) {
                if (line.empty()) continue;
                try {
                    // Просто выполняем команды из файла, но БЕЗ записи обратно в файл
                    std::string cmd = get_first_word(line);
                    if (cmd == "CREATE") internal_create(line);
                    else if (cmd == "INSERT") internal_insert(line);
                    count++;
                } catch (...) {
                    // Игнорируем битые строки в логе
                }
            }
            std::cout << "[DB] Recovered " << count << " operations from disk.\n";
        }

        // Запись в журнал
        void persist(const std::string& query) {
            if (wal_writer.is_open()) {
                wal_writer << query << "\n";
                wal_writer.flush(); // Принудительно сбрасываем буфер на диск
            }
        }

    public:
        DatabaseEngine() {
            // 1. Сначала восстанавливаем состояние
            recover();
            
            // 2. Открываем файл для ДОЗАПИСИ (append mode)
            wal_writer.open(WAL_FILE, std::ios::out | std::ios::app);
            if (!wal_writer.is_open()) {
                std::cerr << "[ERROR] Could not open WAL file for writing!\n";
            }
        }

        ~DatabaseEngine() {
            if (wal_writer.is_open()) wal_writer.close();
        }

        // --- Публичный API ---
        
        // is_recovery_mode = true означает, что мы не должны писать в лог (чтобы не дублировать при старте)
        std::string execute_command(std::string query) {
            std::lock_guard<std::mutex> lock(db_mutex);

            try {
                if (!query.empty() && query.back() == ';') query.pop_back();
                std::string command = get_first_word(query);

                // Логика транзакционности:
                // Сначала меняем память. Если упадет ошибка - в файл не пишем.
                // Если память обновилась успешно - пишем в файл.

                if (command == "CREATE") {
                    internal_create(query); 
                    persist(query); // <-- Сохраняем на диск
                    return "Success: Table created.";
                } 
                else if (command == "INSERT") {
                    internal_insert(query);
                    persist(query); // <-- Сохраняем на диск
                    return "Success: Row inserted.";
                } 
                else if (command == "SELECT") {
                    return internal_select(query); // SELECT на диск не пишем
                }

                return "Error: Unknown command.";

            } catch (const std::exception& e) {
                return std::string("Error: ") + e.what();
            }
        }
    };
}
