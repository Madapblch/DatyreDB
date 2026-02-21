#include "network/connection.hpp"

#include <iostream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unistd.h>     // close, read, write
#include <sys/socket.h> // send

namespace datyre {
namespace network {

    // Реализация конструктора
    Connection::Connection(int socket_fd) 
        : socket_fd_(socket_fd), is_active_(true) {
    }

    // Реализация деструктора
    Connection::~Connection() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
        }
    }

    // Запуск обработки соединения
    void Connection::start() {
        // Отправляем приветствие новому клиенту
        std::string welcome = "Welcome to DatyreDB v1.0!\nType 'PING' to test or 'EXIT' to quit.\ndb > ";
        send_data(welcome);

        // Запускаем цикл чтения в этом же потоке (так как метод вызывается из Thread Pool в Server.cpp)
        handle_read();
    }

    // Отправка данных клиенту
    void Connection::send_data(const std::string& data) {
        if (socket_fd_ >= 0) {
            // MSG_NOSIGNAL предотвращает падение сервера, если клиент внезапно отключился
            send(socket_fd_, data.c_str(), data.size(), MSG_NOSIGNAL);
        }
    }

    // Основной цикл чтения запросов
    void Connection::handle_read() {
        std::vector<char> buffer(4096); // Буфер 4KB

        while (is_active_) {
            // Очищаем буфер перед чтением
            std::fill(buffer.begin(), buffer.end(), 0);
            
            // Читаем данные из сокета
            // buffer.size() - 1, чтобы оставить место для нуль-терминатора (на всякий случай)
            ssize_t bytes_read = read(socket_fd_, buffer.data(), buffer.size() - 1);
            
            if (bytes_read <= 0) {
                // Клиент отключился или ошибка сети
                is_active_ = false;
                break;
            }

            // Превращаем в строку
            std::string request(buffer.data());

            // --- TRIMMING (Удаляем мусор: \r, \n, пробелы по краям) ---
            // 1. Удаляем пробелы справа (включая \r\n от Windows)
            request.erase(std::find_if(request.rbegin(), request.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), request.end());

            // 2. Удаляем пробелы слева
            request.erase(request.begin(), std::find_if(request.begin(), request.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));

            // Если прислали пустую строку (просто Enter), показываем промпт снова
            if (request.empty()) {
                send_data("db > ");
                continue;
            }

            // Логируем чистый запрос на сервере
            std::cout << "[Connection " << id_ << "] Request: '" << request << "'" << std::endl;

            // --- ОБРАБОТКА КОМАНД (Временная заглушка, пока не подключен Session/Dispatcher) ---
            std::string response;

            // Приводим к верхнему регистру для сравнения (для удобства)
            std::string cmd_upper = request;
            std::transform(cmd_upper.begin(), cmd_upper.end(), cmd_upper.begin(), ::toupper);

            if (cmd_upper == "PING") {
                response = "PONG\n";
            } 
            else if (cmd_upper == "EXIT" || cmd_upper == "QUIT") {
                send_data("Goodbye!\n");
                is_active_ = false;
                break;
            } 
            else {
                // Эхо-ответ (показываем, что мы услышали)
                response = "Unknown command: " + request + "\n";
            }

            // Отправляем ответ и приглашение к следующей команде
            send_data(response + "db > ");
        }
    }

} // namespace network
} // namespace datyre
