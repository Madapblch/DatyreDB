cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(DatyreDB 
    VERSION 1.0.0 
    DESCRIPTION "Professional Embedded & Network JSON DBMS"
    LANGUAGES CXX)

# --- 1. СТАНДАРТЫ И ГЛОБАЛЬНЫЕ НАСТРОЙКИ ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Для работы подсказок в Cursor/VSCode

# --- 2. АВТОМАТИЗАЦИЯ ЗАВИСИМОСТЕЙ (Modern FetchContent) ---
# Профессионалы не качают библиотеки руками. CMake сделает это сам.
include(FetchContent)

# Загружаем JSON (nlohmann)
FetchContent_Declare(json URL https://github.com)
FetchContent_MakeAvailable(json)

# Загружаем сетевую библиотеку Asio (Standalone)
FetchContent_Declare(asio URL https://github.com)
FetchContent_MakeAvailable(asio)

# --- 3. СТРОГИЙ КОНТРОЛЬ КОДА (Warnings as Errors) ---
# Чтобы код был чистым, мы превращаем предупреждения в ошибки
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O3)
endif()

# --- 4. МОДУЛЬНАЯ АРХИТЕКТУРА (Targets) ---

# Модуль Ядра (Core) - Твои Database.cpp, Table.cpp, Status.cpp
file(GLOB_RECURSE CORE_SRC "src/core/*.cpp")
add_library(datyredb_core STATIC ${CORE_SRC})
target_include_directories(datyredb_core PUBLIC include)
target_link_libraries(datyredb_core PRIVATE nlohmann_json::nlohmann_json)

# Модуль Сети и Команд (Network & Commands)
file(GLOB_RECURSE NET_SRC "src/network/*.cpp" "src/network/commands/*.cpp")
add_library(datyredb_net STATIC ${NET_SRC})
target_link_libraries(datyredb_net 
    PUBLIC datyredb_core
    PRIVATE asio
)
target_compile_definitions(datyredb_net PUBLIC ASIO_STANDALONE)

# --- 5. ГЛАВНЫЙ ИСПОЛНЯЕМЫЙ ФАЙЛ (Server) ---
add_executable(datyredb_server src/main.cpp)
target_link_libraries(datyredb_server PRIVATE datyredb_net)

# Для многопоточности (ThreadPool) на Linux
find_package(Threads REQUIRED)
target_link_libraries(datyredb_server PRIVATE Threads::Threads)

# Системные библиотеки Windows для сокетов
if(WIN32)
    target_link_libraries(datyredb_server PRIVATE ws2_32 mswsock)
endif()

# --- 6. ОРГАНИЗАЦИЯ (IDE Folders) ---
set_target_properties(datyredb_core PROPERTIES FOLDER "Engine")
set_target_properties(datyredb_net PROPERTIES FOLDER "Network")
set_target_properties(datyredb_server PROPERTIES FOLDER "Apps")

message(STATUS "DatyreDB: Ultra-Pro build system ready.")
