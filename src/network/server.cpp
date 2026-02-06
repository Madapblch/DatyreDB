#include "server.h"

#include <iostream>
#include <cstring>
#include <sstream>

#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#endif

void Server::CloseSocket(socket_t s) {
#if defined(_WIN32)
    closesocket(s);
#else
    close(s);
#endif
}

Server::Server(Database* db, int port)
    : db_(db), port_(port), server_socket_(kInvalidSocket) {
#if defined(_WIN32)
    // Initialize Winsock (Windows only)
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed\n";
        return;
    }
#endif
}

Server::~Server() {
    Stop();
#if defined(_WIN32)
    WSACleanup();
#endif
}

void Server::Run() {
    // Create socket
    server_socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == kInvalidSocket) {
        std::cerr << "socket creation failed\n";
        return;
    }

    // Allow quick rebinding to the same port after restart.
    {
        int opt = 1;
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&opt), sizeof(opt));
    }

    // Bind to port
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    server_addr.sin_addr.s_addr = INADDR_ANY;

#if defined(_WIN32)
    const int bind_rc = bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (bind_rc == SOCKET_ERROR) {
#else
    const int bind_rc = bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (bind_rc != 0) {
#endif
        std::cerr << "bind failed\n";
        CloseSocket(server_socket_);
        return;
    }

    // Listen
#if defined(_WIN32)
    if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR) {
#else
    if (listen(server_socket_, SOMAXCONN) != 0) {
#endif
        std::cerr << "listen failed\n";
        CloseSocket(server_socket_);
        return;
    }

    running_ = true;
    std::cout << "DatyreDB Server listening on port " << port_ << "...\n";

    // Accept clients
    while (running_) {
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        socket_t client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_socket == kInvalidSocket) {
            if (running_) {
                std::cerr << "accept failed\n";
            }
            break;
        }

        char ipbuf[INET_ADDRSTRLEN] = {0};
        const char* ip = inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));
        std::cout << "Client connected from " << (ip ? ip : "unknown") << "\n";

        // Limit number of concurrent clients to avoid unbounded thread growth.
        int current = active_clients_.load(std::memory_order_relaxed);
        if (current >= max_clients_) {
            std::cerr << "Too many clients (" << current << "), rejecting new connection\n";
            CloseSocket(client_socket);
            continue;
        }

        active_clients_.fetch_add(1, std::memory_order_relaxed);
        // Handle client in a new detached thread
        std::thread(&Server::HandleClient, this, client_socket).detach();
    }
}

void Server::Stop() {
    running_ = false;
    if (server_socket_ != kInvalidSocket) {
        CloseSocket(server_socket_);
        server_socket_ = kInvalidSocket;
    }
}

void Server::HandleClient(socket_t client_socket) {
    try {
        // Send welcome message
        SendMessage(client_socket, "DatyreDB v0.1 (server mode)\n");

        while (running_) {
            // Receive command
            std::string command = ReceiveMessage(client_socket);
            if (command.empty()) {
                break; // Client disconnected
            }

            std::cout << "Executing: " << command << "\n";

            // Lock database for thread-safe access, capture textual result.
            std::string response;
            {
                std::lock_guard<std::mutex> lock(db_->db_mutex_);
                std::ostringstream out;
                auto* old_buf = std::cout.rdbuf(out.rdbuf());
                Status status = db_->Execute(command);
                std::cout.rdbuf(old_buf);

                response = out.str();
                // Append status line so client reliably sees result.
                response += (status == Status::OK) ? "OK\n" : "ERROR\n";
            }

            SendMessage(client_socket, response);
        }
    } catch (const std::exception& e) {
        std::cerr << "Client handler error: " << e.what() << "\n";
    }

    std::cout << "Client disconnected\n";
    CloseSocket(client_socket);
    active_clients_.fetch_sub(1, std::memory_order_relaxed);
}

void Server::SendMessage(socket_t socket, const std::string& message) {
#if defined(_WIN32)
    const int rc = send(socket, message.c_str(), static_cast<int>(message.length()), 0);
    if (rc == SOCKET_ERROR) {
#else
    const ssize_t rc = send(socket, message.c_str(), message.length(), 0);
    if (rc < 0) {
#endif
        std::cerr << "send failed\n";
    }
}

std::string Server::ReceiveMessage(socket_t socket) {
    char buffer[4096] = {0};
#if defined(_WIN32)
    const int bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received == SOCKET_ERROR) {
        std::cerr << "recv failed\n";
        return "";
    }
#else
    const ssize_t bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        // Treat interrupted syscall as a retry by returning empty (disconnect-ish)
        // for now; production would handle EINTR/EAGAIN properly.
        std::cerr << "recv failed: " << std::strerror(errno) << "\n";
        return "";
    }
#endif

    if (bytes_received == 0) {
        return ""; // Connection closed
    }

    buffer[static_cast<size_t>(bytes_received)] = '\0';
    std::string message(buffer);

    // Remove trailing newline
    if (!message.empty() && message.back() == '\n') {
        message.pop_back();
    }
    if (!message.empty() && message.back() == '\r') {
        message.pop_back();
    }

    return message;
}
