#include "utils/logger.hpp"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vector>

namespace datyredb {

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::init(const std::string& log_file, LogLevel level) {
    try {
        std::vector<spdlog::sink_ptr> sinks;
        
        // Console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(static_cast<spdlog::level::level_enum>(level));
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        sinks.push_back(console_sink);
        
        // File sink
        if (!log_file.empty()) {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
            sinks.push_back(file_sink);
        }
        
        logger_ = std::make_shared<spdlog::logger>("datyredb", sinks.begin(), sinks.end());
        logger_->set_level(static_cast<spdlog::level::level_enum>(level));
        logger_->flush_on(spdlog::level::warn);
        
        spdlog::set_default_logger(logger_);
        
    } catch (const spdlog::spdlog_ex& ex) {
        // Fallback to console only
        logger_ = spdlog::stdout_color_mt("datyredb");
    }
}

} // namespace datyredb
