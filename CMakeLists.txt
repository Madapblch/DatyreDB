cmake_minimum_required(VERSION 3.16)
project(DatyreDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Suppress MSVC deprecated warnings
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
add_compile_definitions(_WINSOCK_DEPRECATED_NO_WARNINGS)

add_executable(datyredb
    src/main.cpp
    src/db/database.cpp
    src/storage/disk_manager.cpp
    src/buffer/buffer_pool.cpp
    src/storage/wal.cpp
    src/network/server.cpp
)

# Link Winsock on Windows
if(WIN32)
    target_link_libraries(datyredb ws2_32)
endif()

# Smoke tests (no external deps)
add_executable(datyredb_smoke
    src/tests/crash_recovery_smoke.cpp
    src/db/database.cpp
    src/storage/disk_manager.cpp
    src/buffer/buffer_pool.cpp
    src/storage/wal.cpp
)
