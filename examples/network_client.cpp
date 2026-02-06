#include <iostream>
#include <string>
#include <datyredb/client.h> // Предположим, ты напишешь легкий клиент

int main() {
    const std::string server_addr = "127.0.0.1";
    const int port = 7474;

    std::cout << "Подключение к DatyreDB Server по адресу " << server_addr << ":" << port << "..." << std::endl;

    try {
        datyredb::Client client(server_addr, port);

        // Отправка команды в текстовом или бинарном протоколе
        std::string response = client.execute("SELECT * FROM users WHERE id = 1");

        std::cout << "Ответ от сервера: " << response << std::endl;
    } 
    catch (const std::exception& e) {
        std::cerr << "Ошибка подключения: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}