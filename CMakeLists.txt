cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(DatyreDB 
    VERSION 1.0.0 
    DESCRIPTION "High-performance embedded JSON DBMS"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. Автоматическое управление зависимостями (Modern Pro) ---
include(FetchContent)

# Автозагрузка nlohmann_json
FetchContent_Declare(json URL https://github.com)
FetchContent_MakeAvailable(json)

# Автозагрузка Asio (Standalone)
FetchContent_Declare(asio URL https://github.com)
FetchContent_MakeAvailable(asio)

# --- 2. Глобальные флаги компилятора ---
if(MSVC)
    add_compile_options(/W4 /WX /Zc:__cplusplus /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O3)
endif()

# --- 3. Определение модулей (Targets) ---

# Модуль Ядра (Core)
file(GLOB_RECURSE CORE_SRC "src/core/*.cpp")
add_library(datyredb_core STATIC ${CORE_SRC})
target_include_directories(datyredb_core PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(datyredb_core PRIVATE nlohmann_json::nlohmann_json)

# Модуль Сети (Network)
file(GLOB_RECURSE NET_SRC "src/network/*.cpp")
add_library(datyredb_net STATIC ${NET_SRC})
target_link_libraries(datyredb_net 
    PUBLIC datyredb_core
    PRIVATE asio
)
target_compile_definitions(datyredb_net PUBLIC ASIO_STANDALONE)

# --- 4. Исполняемый серверный файл ---
add_executable(datyredb_server src/main.cpp)
target_link_libraries(datyredb_server PRIVATE datyredb_net)

if(WIN32)
    target_link_libraries(datyredb_server PRIVATE ws2_32 mswsock)
endif()

# --- 5. Организация структуры в IDE (Чтобы было красиво) ---
set_target_properties(datyredb_core PROPERTIES FOLDER "Engine")
set_target_properties(datyredb_net PROPERTIES FOLDER "Network")
set_target_properties(datyredb_server PROPERTIES FOLDER "App")

# --- 6. Автоматическая генерация файлов установки ---
include(GNUInstallDirs)
install(TARGETS datyredb_server datyredb_core datyredb_net
    EXPORT DatyreDBTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message(STATUS "DatyreDB: Ultra-Pro build system initialized.")
