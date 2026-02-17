// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  DatyreDB - Logger Header                                                    ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#pragma once

#include <string_view>
#include <fmt/core.h>

namespace datyredb::log {

void debug(std::string_view message);
void info(std::string_view message);
void warn(std::string_view message);
void error(std::string_view message);

template<typename... Args>
void debug(fmt::format_string<Args...> fmt, Args&&... args) {
    debug(fmt::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
void info(fmt::format_string<Args...> fmt, Args&&... args) {
    info(fmt::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
void warn(fmt::format_string<Args...> fmt, Args&&... args) {
    warn(fmt::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
void error(fmt::format_string<Args...> fmt, Args&&... args) {
    error(fmt::format(fmt, std::forward<Args>(args)...));
}

} // namespace datyredb::log
