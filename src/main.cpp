#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>

// Third-party includes
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

// Project includes
#include "core/database.hpp"
#include "network/telegram_bot.hpp"
#include "utils/logger.hpp"
#include "version.hpp"

// ============================================================================
// Global State
// ============================================================================
namespace {
    std::atomic<bool> g_running{true};
    std::atomic<bool> g_reload_config{false};
    std::unique_ptr<datyredb::Database> g_database;
    std::unique_ptr<datyredb::network::TelegramBot> g_telegram_bot;
}

// ============================================================================
// Signal Handlers
// ============================================================================
extern "C" {
    void signal_handler(int sig) {
        if (sig == SIGINT || sig == SIGTERM) {
            static std::atomic<int> shutdown_count{0};
            if (++shutdown_count == 1) {
                g_running = false;
            } else if (shutdown_count >= 2) {
                std::quick_exit(1);
            }
        } else if (sig == SIGHUP) {
            g_reload_config = true;
        }
    }
}

// ============================================================================
// Configuration Structure
// ============================================================================
struct AppConfig {
    // Database settings
    std::string data_dir = "./data";
    std::string config_file = "";
    int port = 5432;
    size_t buffer_pool_size = 1024;
    size_t max_connections = 100;
    
    // Logging settings
    std::string log_file = "datyredb.log";
    std::string log_level = "info";
    size_t max_log_size = 10 * 1024 * 1024;
    size_t max_log_files = 5;
    
    // Telegram settings
    std::string telegram_token = "";
    bool enable_telegram = true;
    
    // Performance settings
    size_t worker_threads = std::thread::hardware_concurrency();
    
    // Debug settings
    bool verbose = false;
    bool daemon_mode = false;
    bool dry_run = false;
    
    // Load from file
    bool load_from_file(const std::string& path) {
        if (!std::filesystem::exists(path)) {
            return false;
        }
        
        std::ifstream file(path);
        std::string line;
        
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespace
            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t"));
                s.erase(s.find_last_not_of(" \t") + 1);
            };
            
            trim(key);
            trim(value);
            
            if (key == "data_dir") data_dir = value;
            else if (key == "port") port = std::stoi(value);
            else if (key == "buffer_pool_size") buffer_pool_size = std::stoull(value);
            else if (key == "log_level") log_level = value;
            else if (key == "log_file") log_file = value;
            else if (key == "worker_threads") worker_threads = std::stoull(value);
        }
        
        return true;
    }
};

// ============================================================================
// Utility Functions
// ============================================================================
void print_banner() {
    std::cout << "\033[1;36m";
    std::cout << R"(
    ╔════════════════════════════════════════════════════════════╗
    ║     ██████╗  █████╗ ████████╗██╗   ██╗██████╗ ███████╗    ║
    ║     ██╔══██╗██╔══██╗╚══██╔══╝╚██╗ ██╔╝██╔══██╗██╔════╝    ║
    ║     ██║  ██║███████║   ██║    ╚████╔╝ ██████╔╝█████╗      ║
    ║     ██║  ██║██╔══██║   ██║     ╚██╔╝  ██╔══██╗██╔══╝      ║
    ║     ██████╔╝██║  ██║   ██║      ██║   ██║  ██║███████╗    ║
    ║     ╚═════╝ ╚═╝  ╚═╝   ╚═╝      ╚═╝   ╚═╝  ╚═╝╚══════╝    ║
    ║                                                            ║
    ║                    Version )" << DATYREDB_VERSION << R"(                        ║
    ║             High-Performance Database System               ║
    ╚════════════════════════════════════════════════════════════╝
)" << "\033[0m" << std::endl;
}

void setup_logging(const AppConfig& config) {
    try {
        std::vector<spdlog::sink_ptr> sinks;
        
        // Console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        sinks.push_back(console_sink);
        
        // File sink
        if (!config.log_file.empty()) {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.log_file, 
                config.max_log_size, 
                config.max_log_files
            );
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
            sinks.push_back(file_sink);
        }
        
        auto logger = std::make_shared<spdlog::logger>("datyredb", sinks.begin(), sinks.end());
        
        // Set log level
        spdlog::level::level_enum level = spdlog::level::info;
        if (config.log_level == "trace") level = spdlog::level::trace;
        else if (config.log_level == "debug") level = spdlog::level::debug;
        else if (config.log_level == "info") level = spdlog::level::info;
        else if (config.log_level == "warn") level = spdlog::level::warn;
        else if (config.log_level == "error") level = spdlog::level::err;
        
        logger->set_level(level);
        logger->flush_on(spdlog::level::warn);
        
        spdlog::set_default_logger(logger);
        spdlog::flush_every(std::chrono::seconds(3));
        
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

void setup_signal_handlers() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
    
    signal(SIGPIPE, SIG_IGN);
}

// ============================================================================
// Main Application Logic
// ============================================================================
int run_application(const AppConfig& config) {
    spdlog::info("Starting DatyreDB v{}", DATYREDB_VERSION);
    spdlog::info("Build: {} with {}", DATYREDB_BUILD_TYPE, DATYREDB_COMPILER);
    
    // Create data directory
    std::filesystem::create_directories(config.data_dir);
    spdlog::info("Data directory: {}", std::filesystem::absolute(config.data_dir).string());
    
    // Initialize database
    spdlog::info("Initializing database engine...");
    g_database = std::make_unique<datyredb::Database>();
    
    if (!g_database->initialize()) {
        spdlog::critical("Failed to initialize database engine");
        return EXIT_FAILURE;
    }
    
    spdlog::info("Database engine initialized successfully");
    
    // Get statistics
    auto stats = g_database->get_statistics();
    spdlog::info("Database loaded: {} tables, {} records", 
        stats.table_count, stats.total_records);
    
    // Initialize Telegram bot
    if (!config.telegram_token.empty() && config.enable_telegram) {
        spdlog::info("Initializing Telegram bot...");
        
        try {
            g_telegram_bot = std::make_unique<datyredb::network::TelegramBot>(
                config.telegram_token, 
                g_database.get()
            );
            g_telegram_bot->start();
            spdlog::info("Telegram bot started successfully");
            
        } catch (const std::exception& e) {
            spdlog::error("Failed to start Telegram bot: {}", e.what());
        }
    } else {
        spdlog::info("Telegram bot disabled (no token provided)");
    }
    
    spdlog::info("System ready. Press Ctrl+C to shutdown.");
    
    // Main loop
    auto last_stats_time = std::chrono::steady_clock::now();
    const auto stats_interval = std::chrono::minutes(5);
    
    while (g_running) {
        // Config reload
        if (g_reload_config) {
            spdlog::info("Reloading configuration...");
            g_reload_config = false;
        }
        
        // Periodic stats
        auto now = std::chrono::steady_clock::now();
        if (now - last_stats_time > stats_interval) {
            auto db_stats = g_database->get_detailed_statistics();
            spdlog::info("Stats: {} qps, {:.2f}ms avg, {:.1f}% cache hits",
                db_stats.queries_per_second,
                db_stats.avg_query_time,
                db_stats.cache_hit_ratio
            );
            last_stats_time = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Shutdown
    spdlog::info("Initiating shutdown...");
    
    if (g_telegram_bot) {
        spdlog::info("Stopping Telegram bot...");
        g_telegram_bot->stop();
        g_telegram_bot.reset();
    }
    
    if (g_database) {
        spdlog::info("Shutting down database...");
        g_database->shutdown();
        g_database.reset();
    }
    
    spdlog::info("Shutdown complete. Goodbye!");
    spdlog::shutdown();
    
    return EXIT_SUCCESS;
}

// ============================================================================
// Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    AppConfig config;
    
    // Parse command line
    CLI::App app{"DatyreDB - High-Performance Database System"};
    
    // Database options
    app.add_option("-d,--data-dir", config.data_dir, 
        "Data directory path")
        ->envname("DATYREDB_DATA_DIR");
    
    app.add_option("-c,--config", config.config_file, 
        "Configuration file path")
        ->envname("DATYREDB_CONFIG");
    
    app.add_option("-p,--port", config.port, 
        "Server port")
        ->envname("DATYREDB_PORT");
    
    // Logging options
    app.add_option("-l,--log-level", config.log_level, 
        "Log level (trace/debug/info/warn/error)")
        ->envname("DATYREDB_LOG_LEVEL");
    
    app.add_option("--log-file", config.log_file, 
        "Log file path")
        ->envname("DATYREDB_LOG_FILE");
    
    // Telegram options
    app.add_option("-t,--telegram-token", config.telegram_token, 
        "Telegram bot token")
        ->envname("TELEGRAM_BOT_TOKEN");
    
    // ИСПРАВЛЕНО: правильный синтаксис для CLI11
    app.add_flag("--no-telegram", [&config](int64_t) { 
        config.enable_telegram = false; 
    }, "Disable Telegram bot integration");
    
    // Performance options
    app.add_option("-w,--workers", config.worker_threads, 
        "Number of worker threads");
    
    app.add_option("-b,--buffer-pool", config.buffer_pool_size, 
        "Buffer pool size in pages");
    
    // Other options
    app.add_flag("-v,--verbose", config.verbose, 
        "Enable verbose output");
    
    app.add_flag("--daemon", config.daemon_mode, 
        "Run as daemon");
    
    app.add_flag("--dry-run", config.dry_run, 
        "Validate configuration and exit");
    
    // Version flag
    app.add_flag_callback("--version", []() {
        std::cout << "DatyreDB version " << DATYREDB_VERSION << std::endl;
        std::cout << "Build type: " << DATYREDB_BUILD_TYPE << std::endl;
        std::cout << "Compiler: " << DATYREDB_COMPILER << std::endl;
        std::exit(0);
    }, "Show version information");
    
    // Parse
    CLI11_PARSE(app, argc, argv);
    
    // Load config file
    if (!config.config_file.empty()) {
        if (!config.load_from_file(config.config_file)) {
            std::cerr << "Failed to load config: " << config.config_file << std::endl;
            return EXIT_FAILURE;
        }
    }
    
    // Get token from environment if not set
    if (config.telegram_token.empty()) {
        const char* env_token = std::getenv("TELEGRAM_BOT_TOKEN");
        if (env_token) {
            config.telegram_token = env_token;
        }
    }
    
    // Setup logging
    setup_logging(config);
    
    // Print banner
    if (!config.daemon_mode && !config.dry_run) {
        print_banner();
    }
    
    // Dry run
    if (config.dry_run) {
        spdlog::info("Configuration validated successfully");
        spdlog::info("  Data directory: {}", config.data_dir);
        spdlog::info("  Port: {}", config.port);
        spdlog::info("  Buffer pool: {} pages", config.buffer_pool_size);
        spdlog::info("  Telegram: {}", config.enable_telegram ? "enabled" : "disabled");
        return EXIT_SUCCESS;
    }
    
    // Daemon mode
    if (config.daemon_mode) {
        spdlog::info("Starting in daemon mode");
        if (daemon(0, 0) != 0) {
            spdlog::critical("Failed to daemonize: {}", strerror(errno));
            return EXIT_FAILURE;
        }
    }
    
    // Setup signals
    setup_signal_handlers();
    
    // Run
    try {
        return run_application(config);
    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    }
}
