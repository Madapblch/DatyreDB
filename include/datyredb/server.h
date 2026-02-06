#ifndef DATYREDB_SERVER_H
#define DATYREDB_SERVER_H

#include <string>
#include <memory>
#include <asio.hpp> // Тебе нужно будет скачать эту библиотеку (header-only)
#include "datyredb/database.h"

namespace datyredb {

class Server {
public:
    // Конструктор принимает порт и ссылку на инициализированную базу
    Server(uint16_t port, Database& db);

    // Запуск цикла прослушивания порта
    void run();

private:
    // Метод для обработки одного подключения
    void handle_client(asio::ip::tcp::socket socket);

    uint16_t port_;
    Database& db_;
    asio::io_context io_context_;
};

} // namespace datyredb

#endif