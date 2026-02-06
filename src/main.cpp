#include "db/database.h"
#include "network/server.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    bool server_mode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--server" || arg == "-s")
            server_mode = true;
    }

    Database db;

    // By default: interactive REPL so you can write in your DB immediately.
    if (!server_mode) {
        std::cout << "DatyreDB v0.1 - Interactive (REPL)\n";
        std::cout << "Type SQL-like commands. EXIT; or Ctrl+C to quit.\n";
        std::cout << "  CREATE DATABASE name;  USE name;\n";
        std::cout << "  CREATE TABLE t (id INT, name VARCHAR);\n";
        std::cout << "  INSERT INTO t VALUES (1, 'hello');\n";
        std::cout << "  SELECT * FROM t;  SHOW TABLES;  DESCRIBE t;\n\n";
        std::string line;
        while (true) {
            std::cout << "datyredb> ";
            if (!std::getline(std::cin, line)) break;
            if (line.empty()) continue;
            Status s = db.Execute(line);
            if (s == Status::ERROR) { /* message already printed */ }
        }
        std::cout << "Bye.\n";
        return 0;
    }

    Server server(&db, 5432);
    std::cout << "DatyreDB v0.1 - Professional Edition\n";
    std::cout << "Starting server on port 5432...\n";
    std::cout << "Clients can connect using TCP (telnet, Python, etc.)\n";
    std::cout << "For interactive REPL, run: datyredb (without --server)\n\n";
    std::cout << "  CREATE DATABASE testdb;  USE testdb;\n";
    std::cout << "  CREATE TABLE users (id INT, name VARCHAR);\n";
    std::cout << "  INSERT INTO users VALUES (1, 'Alice');\n";
    std::cout << "  SELECT * FROM users;\n";
    std::cout << "\nPress Ctrl+C to stop the server.\n\n";

    server.Run();
    return 0;
}