<div align="center">

# 🗄️ DatyreDB

**High-Performance Asynchronous Relational Database Engine written in modern C++17.**

[![C++17](https://img.shields.io/badge/C++-17-blue.svg?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.15+-success.svg?style=flat&logo=cmake)](https://cmake.org/)
[![Docker](https://img.shields.io/badge/Docker-Ready-2496ED.svg?style=flat&logo=docker)](https://www.docker.com/)
[![Boost](https://img.shields.io/badge/Boost-Asio-orange.svg?style=flat&logo=boost)](https://www.boost.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

</div>

---

## 📖 Overview

**DatyreDB** is a lightweight, highly concurrent, and embeddable database engine built from scratch. It is designed to demonstrate advanced systems engineering concepts, including asynchronous network I/O, custom SQL parsing, and persistent storage mechanisms like Write-Ahead Logging (WAL).

This is not just a wrapper around SQLite. It features its own TCP server, Lexer, Parser, and Storage engine.

## ✨ Key Features

- ⚡ **Asynchronous Networking:** Built on top of `Boost.Asio` using the Proactor pattern. Can handle thousands of concurrent TCP connections efficiently.
- 🧵 **Thread-Safe Core:** Implements Reader-Writer locks (`std::shared_mutex`) and fine-grained locking for high-concurrency environments.
- 🔍 **Custom SQL Parser:** Includes a hand-written Lexer, Parser, and Abstract Syntax Tree (AST) to process SQL-like syntax without third-party parser generators.
- 💾 **Storage Engine (WIP):** Features a Buffer Pool Manager, Disk Manager, and Write-Ahead Logging (WAL) for ACID compliance.
- 🐳 **Production-Ready Deployment:** Multi-stage Dockerized environment running as a non-root user.
- 🤖 **Telegram Bot API:** Built-in integration for remote monitoring and statistics (via `libcurl` and `nlohmann/json`).

---

## 🏗️ Architecture

```text
Client (TCP)
   │
   ▼
[ Network Layer (Boost.Asio) ] ──▶ [ Telegram Bot (Stats & Monitoring) ]
   │
   ▼
[ Session & Dispatcher ]
   │
   ▼
[ Query Executor ]
   │
   ├──▶ [ SQL Parser (Lexer -> AST) ]
   │
   ▼
[ Storage Engine ]
   ├──▶ Buffer Pool Manager
   ├──▶ Write-Ahead Log (WAL)
   └──▶ Disk Manager (.db files)
🚀 Quick Start (Docker)
The easiest way to run DatyreDB is using Docker Compose. The image is optimized and contains only the runtime binaries.

Bash

# 1. Clone the repository
git clone https://github.com/Madapblch/DatyreDB.git
cd DatyreDB

# 2. Build and start the server in detached mode
docker compose up -d --build

# 3. Check the logs to ensure the server is running
docker logs -f datyre_server
💻 Building from Source (Linux)
If you want to build the project locally for development.

Prerequisites
GCC 9+ or Clang 10+ (Must support C++17)
CMake 3.15+
Boost (System, Thread)
libcurl, libfmt, libspdlog, nlohmann-json
Build Instructions
Bash

mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
🔌 Connecting & Usage
Once the server is running on port 7432, you can connect to it using any raw TCP client like telnet or nc (Netcat).

Bash

telnet 127.0.0.1 7432
(Note for Windows users: Press Ctrl+], type set localecho, and press Enter twice to see your input).

Interactive Shell
text

Welcome to DatyreDB (Async Pro Edition)!
Type 'EXIT' to quit.

db > PING
PONG
db > CREATE TABLE users (id, name, role)
Table 'users' created
db > EXIT
Goodbye!
🗺️ Roadmap
 Non-blocking TCP Server (Boost.Asio)
 Basic SQL Parser (AST)
 Multi-stage Docker deployment
 B-Tree / Hash Indexing implementation
 Advanced SQL (JOINs, WHERE clauses)
 MVCC (Multi-Version Concurrency Control)
 Client SDKs (Python, Go)
🛠️ Tech Stack & Dependencies
DatyreDB stands on the shoulders of giants. We use the following excellent Open Source libraries:

Boost.Asio - Asynchronous I/O.
spdlog - Extremely fast C++ logging library.
fmt - Modern formatting library.
nlohmann/json - JSON for Modern C++.
libcurl - URL transfer library (used for Telegram API).
🤝 Contributing
Contributions are what make the open-source community such an amazing place to learn, inspire, and create. Any contributions you make are greatly appreciated.

Fork the Project
Create your Feature Branch (git checkout -b feature/AmazingFeature)
Commit your Changes (git commit -m 'Add some AmazingFeature')
Push to the Branch (git push origin feature/AmazingFeature)
Open a Pull Request
📄 License
Distributed under the MIT License. See LICENSE for more information.

<div align="center"> <i>Built with ❤️ by <a href="https://github.com/Madapblch">Madapblch</a></i> </div> ```
