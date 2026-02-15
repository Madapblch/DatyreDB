#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace datyredb {

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5
};

class Logger {
public:
    static void init(const std::string& log_file, LogLevel level = LogLevel::INFO);
    
    template<typename... Args>
    static void trace(const std::string& fmt, Args&&... args) {
        if (logger_) logger_->trace(fmt, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void debug(const std::string& fmt, Args&&... args) {
        if (logger_) logger_->debug(fmt, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void info(const std::string& fmt, Args&&... args) {
        if (logger_) logger_->info(fmt, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void warn(const std::string& fmt, Args&&... args) {
        if (logger_) logger_->warn(fmt, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void error(const std::string& fmt, Args&&... args) {
        if (logger_) logger_->error(fmt, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void critical(const std::string& fmt, Args&&... args) {
        if (logger_) logger_->critical(fmt, std::forward<Args>(args)...);
    }
    
private:
    static std::shared_ptr<spdlog::logger> logger_;
};

} // namespace datyredb
