// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Logger Implementation                                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "internal/utils/logger.hpp"
#include "datyredb/config.hpp"

#include <fmt/core.h>
#include <fmt/chrono.h>
#include <fmt/color.h>

#include <chrono>
#include <iostream>
#include <mutex>
#include <ctime>
#include <string_view>

namespace datyredb::log {

namespace {

std::mutex g_log_mutex;

enum class Level { Debug, Info, Warn, Error };

void log_impl(Level level, std::string_view message) {
#if DATYREDB_ENABLE_LOGGING
    std::lock_guard lock(g_log_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    const char* level_str = nullptr;
    fmt::color color = fmt::color::white;
    
    switch (level) {
        case Level::Debug:
            level_str = "DEBUG";
            color = fmt::color::gray;
            break;
        case Level::Info:
            level_str = "INFO ";
            color = fmt::color::green;
            break;
        case Level::Warn:
            level_str = "WARN ";
            color = fmt::color::yellow;
            break;
        case Level::Error:
            level_str = "ERROR";
            color = fmt::color::red;
            break;
    }
    
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif
    
    fmt::print(stderr, "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d} ",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
        static_cast<int>(ms.count()));
    
    fmt::print(stderr, fg(color), "[{}] ", level_str);
    fmt::print(stderr, "{}\n", message);
#else
    (void)level;
    (void)message;
#endif
}

} // anonymous namespace

void debug(std::string_view message) {
    log_impl(Level::Debug, message);
}

void info(std::string_view message) {
    log_impl(Level::Info, message);
}

void warn(std::string_view message) {
    log_impl(Level::Warn, message);
}

void error(std::string_view message) {
    log_impl(Level::Error, message);
}

} // namespace datyredb::log
