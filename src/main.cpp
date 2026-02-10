#include <iostream>
#include <string>
#include "datyredb/sql/lexer.h"
#include "datyredb/sql/parser.h"

using namespace datyredb;

int main() {
    std::cout << "DatyreDB v0.1.0 - Professional SQL Database\n";
    std::cout << "Type 'exit' to quit\n\n";
    
    while (true) {
        std::cout << "datyredb> ";
        
        std::string query;
        std::getline(std::cin, query);
        
        if (query == "exit" || query == "quit") {
            break;
        }
        
        if (query.empty()) {
            continue;
        }
        
        try {
            // Lexical analysis
            sql::Lexer lexer(query);
            auto tokens = lexer.tokenize();
            
            std::cout << "Tokens:\n";
            for (const auto& token : tokens) {
                if (token.type != sql::TokenType::END_OF_FILE) {
                    std::cout << "  " << token.to_string() << "\n";
                }
            }
            
            // Parsing
            sql::Parser parser(std::move(tokens));
            auto stmt = parser.parse();
            
            std::cout << "\nParsed statement:\n";
            std::cout << "  " << stmt->to_string() << "\n\n";
            
            // TODO: Execute statement
            std::cout << "[Execution not yet implemented]\n\n";
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n\n";
        }
    }
    
    std::cout << "Goodbye!\n";
    return 0;
}
