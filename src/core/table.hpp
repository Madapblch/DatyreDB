#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "datyredb/status.hpp" // Обязательно подключаем Status

namespace datyre {

class Table {
public:
    // Конструктор
    explicit Table(std::string name);

    // Методы, которые ты реализуешь в cpp
    Status insert(const nlohmann::json& data);
    bool validate(const nlohmann::json& data);

    // Геттеры
    std::string GetName() const { return name_; }

private:
    std::string name_;
    // Добавь другие поля, если нужно (схема, строки и т.д.)
};

} // namespace datyre
