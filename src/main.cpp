/**
 * @file main.cpp
 * @brief Entry point for DatyreDB Server (Async Edition)
 */

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>

// Сторонние библиотеки
#include <fmt/color.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// Внутренние компоненты
#include "core/database.hpp"
#include "network/server.hpp"

// Глобальный флаг для обработки сигналов (SIGINT/SIGTERM)
std::atomic<bool> g_running{true};

// Обработчик сигналов (вызывается при Ctrl+C)
void signal_handler(int signum) {
    if (g_running) {
        // Используем низкоуровневый вывод, так как аллокации в signal handler небезопасны
        // Но для простоты оставим флаг.
        g_running = false; 
    }
}

// Красивый баннер при запуске
void print_banner() {
    fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold,
        "\n"
        "╔══════════════════════════════════════════════════════╗\n"
        "║               DatyreDB Server v1.0                   ║\n"
        "║   (Async I/O, High-Performance, C++17, Boost)        ║\n"
        "╚══════════════════════════════════════════════════════╝\n\n"
    );
}

// Настройка логгера
void setup_logging() {
    try {
        auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>("datyre", console);
        
        // Формат логов: [Время] [Уровень] Сообщение
        logger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
        logger->set_level(spdlog::level::info);
        
        spdlog::set_default_logger(logger);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log init failed: " << ex.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // 1. Инициализация
    setup_logging();
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    print_banner();
    
    // Парсинг аргументов (простой вариант)
    int port = 7432;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            spdlog::warn("Invalid port argument. Using default: {}", port);
        }
    }

    try {
        // 2. Инициализация ядра базы данных
        spdlog::info("Initializing Database Engine...");
        datyre::Database db; // Здесь может быть загрузка с диска

        // 3. Конфигурация сервера
        datyre::network::ServerConfig server_config;
        server_config.tcp_port = port;
        server_config.max_connections = 5000; // Boost.Asio легко держит тысячи

        spdlog::info("Starting Network Server on port {}...", port);
        datyre::network::Server server(server_config, db);

        // 4. Запуск сервера в отдельном потоке
        // Это нужно, чтобы главный поток мог ждать сигнала остановки
        std::thread server_thread([&server]() {
            try {
                server.run(); // Это блокирующий вызов (io_context.run)
            } catch (const std::exception& e) {
                spdlog::error("Server thread crashed: {}", e.what());
                g_running = false;
            }
        });

        spdlog::info("DatyreDB is ready to accept connections!");

        // 5. Главный цикл ожидания (Main Loop)
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            // Здесь можно выполнять периодические задачи:
            // - Сбор метрик
            // - Checkpointing (сброс WAL)
        }

        // 6. Завершение работы (Graceful Shutdown)
        spdlog::info("Shutdown signal received. Stopping server...");
        
        server.stop(); // Останавливаем io_context

        if (server_thread.joinable()) {
            server_thread.join(); // Ждем завершения сетевых операций
        }

        db.shutdown(); // Сохраняем данные на диск

        spdlog::info("DatyreDB stopped successfully.");

    } catch (const std::exception& e) {
        spdlog::critical("Fatal Error: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        spdlog::critical("Unknown Fatal Error.");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
