#pragma once

#include <string>
#include <memory>

// Forward declaration: Мы говорим компилятору, что класс Database существует,
// но не подключаем тяжелый database.hpp здесь.
namespace datyre {
    class Database;
}

namespace datyre {
namespace network {

    class Dispatcher {
    public:
        // Диспетчер должен знать о базе данных, чтобы передавать ей команды
        explicit Dispatcher(datyre::Database& db);

        // Метод обработки сырого запроса (строки)
        std::string dispatch(const std::string& request);

    private:
        datyre::Database& db_;
    };

} // namespace network
} // namespace datyre
