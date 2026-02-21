#pragma once

#include <string>
#include <algorithm>

namespace datyre {

enum class StatusCode {
    kOk = 0,
    kNotFound = 1,
    kCorruption = 2,
    kNotSupported = 3,
    kInvalidArgument = 4,
    kIOError = 5
};

class Status {
public:
    // Конструкторы
    Status() : code_(StatusCode::kOk), msg_("") {}
    Status(StatusCode code, const std::string& msg) : code_(code), msg_(msg) {}

    // Фабричные методы (Static Factory Methods)
    static Status OK() { return Status(); }
    static Status NotFound(const std::string& msg) { return Status(StatusCode::kNotFound, msg); }
    static Status Corruption(const std::string& msg) { return Status(StatusCode::kCorruption, msg); }
    static Status NotSupported(const std::string& msg) { return Status(StatusCode::kNotSupported, msg); }
    static Status InvalidArgument(const std::string& msg) { return Status(StatusCode::kInvalidArgument, msg); }
    static Status IOError(const std::string& msg) { return Status(StatusCode::kIOError, msg); }

    // Проверки
    bool ok() const { return code_ == StatusCode::kOk; }
    bool IsNotFound() const { return code_ == StatusCode::kNotFound; }

    // Конвертация в строку
    std::string ToString() const {
        if (code_ == StatusCode::kOk) {
            return "OK";
        }
        std::string type;
        switch (code_) {
            case StatusCode::kNotFound: type = "NotFound: "; break;
            case StatusCode::kCorruption: type = "Corruption: "; break;
            case StatusCode::kNotSupported: type = "NotSupported: "; break;
            case StatusCode::kInvalidArgument: type = "InvalidArgument: "; break;
            case StatusCode::kIOError: type = "IOError: "; break;
            default: type = "Unknown: "; break;
        }
        return type + msg_;
    }

private:
    StatusCode code_;
    std::string msg_;
};

} // namespace datyre
