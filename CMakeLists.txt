cmake_minimum_required(VERSION 3.16)

# Описание проекта
project(DatyreDB
    VERSION 1.0.0
    DESCRIPTION "High-Performance Asynchronous Embedded Database Engine"
    HOMEPAGE_URL "https://github.com/Madapblch/DatyreDB"
    LANGUAGES CXX
)

# --- 1. Настройки компилятора ---

# Требуем C++17 (или новее) для современных фич (filesystem, string_view, etc.)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Отключаем расширения компилятора, только стандарт

# Настройки для оптимизации
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Опции предупреждений (Professional: Warnings are friends)
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# --- 2. Зависимости (Dependencies) ---

# Логирование и форматирование
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

# JSON парсер
find_package(nlohmann_json REQUIRED)

# Работа с сетью и HTTP (для бота)
find_package(CURL REQUIRED)

# Асинхронность и потоки (Boost.Asio)
set(BOOST_REQUIRED_COMPONENTS system thread)
find_package(Boost 1.70 REQUIRED COMPONENTS ${BOOST_REQUIRED_COMPONENTS})

# Системные потоки
find_package(Threads REQUIRED)

# --- 3. Сборка Библиотеки (Core Engine) ---

# Собираем все исходники, кроме main.cpp
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
list(FILTER LIB_SOURCES EXCLUDE REGEX "src/main.cpp")

add_library(datyredb_lib STATIC ${LIB_SOURCES})

# Настройка путей для заголовков (чтобы писать #include "core/database.hpp")
target_include_directories(datyredb_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Линковка зависимостей к библиотеке
target_link_libraries(datyredb_lib PUBLIC
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    CURL::libcurl
    Boost::system
    Boost::thread
    Threads::Threads
)

# --- 4. Сборка Исполняемого файла (Server) ---

add_executable(datyredb src/main.cpp)

# Линкуем наше ядро к бинарнику
target_link_libraries(datyredb PRIVATE datyredb_lib)

# --- 5. Установка (Deployment) ---

include(GNUInstallDirs)

# Устанавливаем бинарник в /usr/local/bin
install(TARGETS datyredb
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Устанавливаем библиотеку (если кто-то захочет использовать движок отдельно)
install(TARGETS datyredb_lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
