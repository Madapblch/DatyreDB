#include "network/session.hpp"
#include "core/database.hpp"

#include <iostream>
#include <vector>
#include <boost/algorithm/string.hpp> // trim, replace_all, erase_all

namespace datyre {
namespace network {

    std::shared_ptr<Session> Session::create(tcp::socket socket, datyre::Database& db) {
        return std::make_shared<Session>(std::move(socket), db);
    }

    Session::Session(tcp::socket socket, datyre::Database& db)
        : socket_(std::move(socket)), db_(db) {
    }

    void Session::start() {
        // Формируем приветствие.
        // Используем обычные \n, метод deliver сам превратит их в \r\n
        std::string welcome = 
            "Welcome to DatyreDB (Async Pro Edition)!\n"
            "Type 'EXIT' to quit.\n"
            "\n"
            "db > "; // Промпт в конце без переноса строки
        
        deliver(welcome);
        do_read();
    }

    void Session::deliver(std::string msg) {
        bool write_in_progress = !write_msgs_.empty();
        
        // --- ПРОФЕССИОНАЛЬНАЯ НОРМАЛИЗАЦИЯ (SANITIZATION) ---
        
        // 1. Сначала удаляем ВСЕ символы возврата каретки (\r), 
        //    чтобы избежать дублирования (\r\r\n) или мусора.
        boost::erase_all(msg, "\r");

        // 2. Теперь заменяем все LF (\n) на CRLF (\r\n).
        //    Это стандарт для сетевых терминалов (Telnet/SSH).
        boost::replace_all(msg, "\n", "\r\n");

        write_msgs_.push_back(msg);

        if (!write_in_progress) {
            do_write();
        }
    }

    void Session::do_read() {
        auto self(shared_from_this());
        
        // Читаем до ближайшего переноса строки.
        // Это может быть \n (Linux) или \r\n (Windows).
        boost::asio::async_read_until(socket_, input_buffer_, '\n',
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    std::istream is(&input_buffer_);
                    std::string line;
                    std::getline(is, line); // Извлекает строку из буфера

                    // --- ОЧИСТКА ВХОДЯЩИХ ДАННЫХ ---
                    // Удаляем любые управляющие символы с обоих концов
                    boost::trim_if(line, boost::is_any_of("\r\n\t "));

                    if (!line.empty()) {
                        process_command(line);
                    } else {
                        // Если пользователь просто нажал Enter, обновляем промпт
                        deliver("db > "); 
                    }

                    do_read(); 
                }
                // При ошибке (разрыв связи) сессия уничтожается автоматически
            });
    }

    void Session::do_write() {
        auto self(shared_from_this());
        
        boost::asio::async_write(socket_,
            boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    write_msgs_.pop_front();
                    if (!write_msgs_.empty()) {
                        do_write();
                    }
                }
            });
    }

    void Session::process_command(std::string command) {
        // Логирование на сервере
        std::cout << "[Session] Command: " << command << std::endl;

        std::string response;
        std::string cmd_upper = boost::to_upper_copy(command);

        if (cmd_upper == "PING") {
            response = "PONG\n";
        } 
        else if (cmd_upper == "EXIT" || cmd_upper == "QUIT") {
            auto self(shared_from_this());
            // Отправляем прощание и закрываем сокет после записи
            std::string msg = "Goodbye!\n";
            // Нормализуем вручную тут, т.к. не используем deliver для закрытия
            boost::replace_all(msg, "\n", "\r\n"); 
            
            boost::asio::async_write(socket_, boost::asio::buffer(msg),
                [this, self](boost::system::error_code, std::size_t) {
                    socket_.close();
                });
            return;
        } 
        else {
            // Эхо-ответ (в будущем здесь будет вызов SQL движка)
            response = "Unrecognized command: " + command + "\n";
        }

        // Добавляем приглашение к следующему вводу
        // Обрати внимание: response заканчивается на \n, а после него сразу промпт
        deliver(response + "db > ");
    }

} // namespace network
} // namespace datyre
