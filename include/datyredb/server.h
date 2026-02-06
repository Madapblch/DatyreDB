#ifndef DATYREDB_SERVER_H
#define DATYREDB_SERVER_H

#include <asio.hpp>
#include <memory>
#include <string>
#include "datyredb/database.h"
#include "datyredb/network/dispatcher.h"

namespace datyredb {

/**
 * @brief Профессиональный сетевой сервер для DatyreDB.
 * Работает на базе Asio, принимает TCP-соединения и передает их Диспетчеру.
 */
class Server {
public:
    // Конструктор: связываем сервер с существующим ядром базы
    Server(uint16_t port, Database& db);

    // Запуск бесконечного цикла прослушивания порта
    void run();

private:
    // Обработка одного входящего подключения
    void handle_client(asio::ip::tcp::socket socket);

    uint16_t port_;
    Database& db_;
    asio::io_context io_context_;
    
    // "Мозг" сервера, который знает все команды
    CommandDispatcher dispatcher_;
};

} // namespace datyredb

#endif // DATYREDB_SERVER_H