cmake_minimum_required(VERSION 3.16)
project(DatyreDB 
    VERSION 0.1.0
    DESCRIPTION "High-performance embedded database"
    LANGUAGES CXX
)

# Стандарт C++20 (стабильный для production)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опции компиляции для разных режимов
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Флаги компиляции
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

# Собираем исходники
file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/network/*.cpp"
)

# Главная цель
add_executable(${PROJECT_NAME} ${SOURCES})

# Пути к заголовкам
target_include_directories(${PROJECT_NAME} 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Линковка библиотек (если нужны)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)