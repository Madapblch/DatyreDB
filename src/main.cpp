#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <fmt/core.h>
#include <fmt/color.h>

#include "core/engine.hpp"

constexpr int PORT = 7432;
constexpr int BUFFER_SIZE = 4096;

namespace {

    void print_banner() {
        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold,
            "╔════════════════════════════════════════════════╗\n"
            "║             DatyreDB Server v1.1               ║\n"
            "║   (Persistent AOF, Multi-threaded, C++17)      ║\n"
            "╚════════════════════════════════════════════════╝\n\n"
        );
    }

    void handle_client(int client_socket, datyre::DatabaseEngine& db) {
        char buffer[BUFFER_SIZE] = {0};
        std::string welcome = "Connected to DatyreDB (Persistent).\ndb > ";
        send(client_socket, welcome.c_str(), welcome.length(), 0);

        while (true) {
            std::memset(buffer, 0, BUFFER_SIZE);
            ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
            if (bytes_read <= 0) break;

            std::string command(buffer);
            while (!command.empty() && std::isspace(static_cast<unsigned char>(command.back()))) {
                command.pop_back();
            }

            if (command.empty()) {
                send(client_socket, "db > ", 5, 0);
                continue;
            }

            fmt::print("[CLIENT] Executing: {}\n", command);
            std::string response;

            if (command == "exit" || command == "quit") {
                response = "Bye.\n";
                send(client_socket, response.c_str(), response.length(), 0);
                break;
            } else if (command == "ping") {
                response = "pong\n";
            } else {
                response = db.execute_command(command) + "\n";
            }

            response += "db > ";
            send(client_socket, response.c_str(), response.length(), 0);
        }
        close(client_socket);
        fmt::print(fg(fmt::color::yellow), "[INFO] Client disconnected.\n");
    }
}

int main() {
    print_banner();

    // При создании объекта db запустится recover() и загрузит данные из файла
    fmt::print(fg(fmt::color::green), "[INIT] Initializing Database Engine...\n");
    datyre::DatabaseEngine db;

    int server_fd;
    struct sockaddr_in address{};
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        return EXIT_FAILURE;
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        perror("Bind failed");
        return EXIT_FAILURE;
    }
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        return EXIT_FAILURE;
    }

    fmt::print(fg(fmt::color::blue), "[READY] Server listening on port {}\n", PORT);

    while (true) {
        int new_socket = accept(server_fd, reinterpret_cast<struct sockaddr*>(&address), &addrlen);
        if (new_socket < 0) continue;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        fmt::print(fg(fmt::color::green), "[INFO] Connection from {}\n", client_ip);

        std::thread client_thread(handle_client, new_socket, std::ref(db));
        client_thread.detach();
    }
    return EXIT_SUCCESS;
}
