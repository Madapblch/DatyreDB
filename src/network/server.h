#pragma once

#include "../db/database.h"

#include <thread>
#include <memory>
#include <string>
#include <atomic>

#if defined(_WIN32)
#include <winsock2.h>
using socket_t = SOCKET;
static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
#include <sys/socket.h>
using socket_t = int;
static constexpr socket_t kInvalidSocket = -1;
#endif

class Server {
public:
    Server(Database* db, int port = 5432);
    ~Server();

    // Start the server (blocking)
    void Run();

    // Stop the server
    void Stop();

private:
    Database* db_;
    int port_;
    socket_t server_socket_;
    bool running_{false};

    std::atomic<int> active_clients_{0};
    int max_clients_{64};

    // Handle a single client connection
    void HandleClient(socket_t client_socket);

    // Helper to send message to client
    static void SendMessage(socket_t socket, const std::string& message);

    // Helper to receive message from client
    static std::string ReceiveMessage(socket_t socket);

    static void CloseSocket(socket_t s);
};
