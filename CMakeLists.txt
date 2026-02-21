cmake_minimum_required(VERSION 3.15)

project(DatyreDB
    VERSION 1.0.0
    DESCRIPTION "High-Performance Embedded Database Engine"
    LANGUAGES CXX
)

# --- 1. Configuration ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization for Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- 2. Dependencies ---
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)

# --- 3. Library (Core Engine) ---
# Собираем все исходники, кроме main.cpp
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
list(FILTER LIB_SOURCES EXCLUDE REGEX "src/main.cpp")

add_library(datyredb_lib STATIC ${LIB_SOURCES})

# Настройка путей для заголовков
target_include_directories(datyredb_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Линковка зависимостей
target_link_libraries(datyredb_lib PUBLIC 
    fmt::fmt 
    spdlog::spdlog 
    Threads::Threads
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

# Жесткие предупреждения компилятора (Quality Control)
target_compile_options(datyredb_lib PRIVATE -Wall -Wextra -Wpedantic)

# --- 4. Executable (Server) ---
add_executable(datyredb src/main.cpp)

target_link_libraries(datyredb PRIVATE datyredb_lib)

# --- 5. Installation ---
include(GNUInstallDirs)
install(TARGETS datyredb datyredb_lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
