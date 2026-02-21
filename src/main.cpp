#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

#include <fmt/color.h>
#include <spdlog/spdlog.h>

#include "core/database.hpp"
#include "network/server.hpp"

// Глобальный флаг остановки
std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

void print_banner() {
    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold,
        "╔════════════════════════════════════════════════╗\n"
        "║             DatyreDB Server v1.0               ║\n"
        "║   (High-Performance, Multi-threaded, NoSQL)    ║\n"
        "╚════════════════════════════════════════════════╝\n\n"
    );
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    print_banner();
    spdlog::info("Initializing DatyreDB...");

    try {
        // 1. Инициализация базы данных
        datyre::Database db;

        // 2. Настройка сервера
        datyre::network::ServerConfig config;
        config.tcp_port = 7432;
        config.max_connections = 1000;

        datyre::network::Server server(config);

        // 3. Запуск
        if (!server.start()) {
            spdlog::critical("Failed to start server!");
            return EXIT_FAILURE;
        }

        spdlog::info("Server is running on port {}. Press Ctrl+C to stop.", config.tcp_port);

        // 4. Главный цикл (Main Loop)
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // 5. Остановка
        spdlog::info("Shutting down...");
        server.stop();

    } catch (const std::exception& e) {
        spdlog::critical("Fatal Error: {}", e.what());
        return EXIT_FAILURE;
    }

    spdlog::info("Goodbye!");
    return EXIT_SUCCESS;
}
