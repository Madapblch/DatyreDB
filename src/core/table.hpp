#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace datyre {

    class Table {
    public:
        std::string name;
        std::vector<std::string> columns;
        std::vector<std::vector<std::string>> rows;

        Table() = default;
        Table(const std::string& table_name, const std::vector<std::string>& cols)
            : name(table_name), columns(cols) {}

        void insert(const std::vector<std::string>& values) {
            if (values.size() != columns.size()) {
                throw std::runtime_error("Column count mismatch");
            }
            rows.push_back(values);
        }

        std::string to_string() const {
            if (columns.empty()) return "Empty Table";

            std::stringstream ss;
            
            // Header
            for (const auto& col : columns) {
                ss << std::left << std::setw(15) << col << "| ";
            }
            ss << "\n";
            ss << std::string(columns.size() * 17, '-') << "\n";

            // Rows
            for (const auto& row : rows) {
                for (const auto& cell : row) {
                    ss << std::left << std::setw(15) << cell << "| ";
                }
                ss << "\n";
            }
            ss << "\n(" << rows.size() << " rows)\n";
            return ss.str();
        }
    };
}
