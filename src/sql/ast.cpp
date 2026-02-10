#include "datyredb/sql/ast.h"
#include <sstream>

namespace datyredb::sql {

// LiteralExpression implementation
std::string LiteralExpression::to_string() const {
    switch (literal_type_) {
        case LiteralType::INTEGER:
            return std::to_string(int_value_);
        case LiteralType::STRING:
            return "'" + string_value_ + "'";
        case LiteralType::NULL_VALUE:
            return "NULL";
    }
    return "UNKNOWN";
}

// BinaryExpression implementation
std::string BinaryExpression::to_string() const {
    std::string op_str;
    switch (op_) {
        case Operator::EQUAL: op_str = "="; break;
        case Operator::NOT_EQUAL: op_str = "!="; break;
        case Operator::LESS: op_str = "<"; break;
        case Operator::LESS_EQUAL: op_str = "<="; break;
        case Operator::GREATER: op_str = ">"; break;
        case Operator::GREATER_EQUAL: op_str = ">="; break;
        case Operator::AND: op_str = "AND"; break;
        case Operator::OR: op_str = "OR"; break;
        case Operator::PLUS: op_str = "+"; break;
        case Operator::MINUS: op_str = "-"; break;
        case Operator::MULTIPLY: op_str = "*"; break;
        case Operator::DIVIDE: op_str = "/"; break;
    }
    
    return "(" + left_->to_string() + " " + op_str + " " + right_->to_string() + ")";
}

// SelectStatement implementation
std::string SelectStatement::to_string() const {
    std::stringstream ss;
    ss << "SELECT ";
    
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << columns_[i]->to_string();
    }
    
    ss << " FROM " << table_name_;
    
    if (where_clause_) {
        ss << " WHERE " << where_clause_->to_string();
    }
    
    return ss.str();
}

// InsertStatement implementation
std::string InsertStatement::to_string() const {
    std::stringstream ss;
    ss << "INSERT INTO " << table_name_;
    
    if (!column_names_.empty()) {
        ss << " (";
        for (size_t i = 0; i < column_names_.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << column_names_[i];
        }
        ss << ")";
    }
    
    ss << " VALUES (";
    for (size_t i = 0; i < values_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << values_[i]->to_string();
    }
    ss << ")";
    
    return ss.str();
}

// UpdateStatement implementation
std::string UpdateStatement::to_string() const {
    std::stringstream ss;
    ss << "UPDATE " << table_name_ << " SET ";
    
    for (size_t i = 0; i < assignments_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << assignments_[i].first << " = " << assignments_[i].second->to_string();
    }
    
    if (where_clause_) {
        ss << " WHERE " << where_clause_->to_string();
    }
    
    return ss.str();
}

// DeleteStatement implementation
std::string DeleteStatement::to_string() const {
    std::stringstream ss;
    ss << "DELETE FROM " << table_name_;
    
    if (where_clause_) {
        ss << " WHERE " << where_clause_->to_string();
    }
    
    return ss.str();
}

// CreateTableStatement implementation
std::string CreateTableStatement::to_string() const {
    std::stringstream ss;
    ss << "CREATE TABLE " << table_name_ << " (";
    
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << columns_[i].name << " " << columns_[i].type;
        if (columns_[i].is_primary_key) {
            ss << " PRIMARY KEY";
        }
    }
    ss << ")";
    
    return ss.str();
}

} // namespace datyredb::sql
