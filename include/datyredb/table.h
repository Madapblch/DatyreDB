#ifndef DATYREDB_TABLE_H
#define DATYREDB_TABLE_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp> 
#include "datyredb/status.h"

namespace datyredb {

class Table {
public:
    Table(std::string name);
    
    // Профессиональный метод вставки (возвращает Status)
    Status insert(const nlohmann::json& data);
    
    std::string getName() const { return name_; }

private:
    bool validate(const nlohmann::json& data); 
    void saveToFile(const nlohmann::json& data);

    std::string name_;
    // Здесь позже добавим логику работы с файлом
};

} // namespace datyredb

#endif