#ifndef DATYREDB_STATUS_H
#define DATYREDB_STATUS_H

#include <string>
#include <string_view>

namespace datyredb {

// Список всех возможных состояний базы
enum class StatusCode {
    OK = 0,
    NOT_FOUND = 1,
    ALREADY_EXISTS = 2,
    INVALID_SCHEMA = 3,
    IO_ERROR = 4,      // Ошибка записи/чтения файла
    CORRUPTED = 5,     // Ошибка парсинга JSON
    INTERNAL_ERROR = 6
};

class Status {
public:
    // Конструкторы для удобства
    Status(StatusCode code) : code_(code), message_("") {}
    Status(StatusCode code, std::string_view msg) : code_(code), message_(msg) {}

    // Статические методы для быстрого создания (как в Google Status)
    static Status OK() { return Status(StatusCode::OK); }
    static Status NotFound(std::string_view msg) { return Status(StatusCode::NOT_FOUND, msg); }
    static Status IOError(std::string_view msg) { return Status(StatusCode::IO_ERROR, msg); }

    // Проверки состояния
    bool ok() const { return code_ == StatusCode::OK; }
    StatusCode code() const { return code_; }
    const std::string& message() const { return message_; }

    // Удобный вывод в строку
    std::string toString() const {
        if (ok()) return "OK";
        return "Error (" + std::to_string(static_cast<int>(code_)) + "): " + message_;
    }

private:
    StatusCode code_;
    std::string message_;
};

} // namespace datyredb

#endif // DATYREDB_STATUS_H