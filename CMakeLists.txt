cmake_minimum_required(VERSION 3.16)
project(DatyreDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compile options
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/sql/lexer.cpp
    src/sql/parser.cpp
    src/sql/ast.cpp
)

# Main executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Google Test (optional, for tests)
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
    
    add_executable(datyredb_tests
        tests/sql/lexer_test.cpp
        tests/sql/parser_test.cpp
        src/sql/lexer.cpp
        src/sql/parser.cpp
        src/sql/ast.cpp
    )
    
    target_link_libraries(datyredb_tests gtest_main)
    
    include(GoogleTest)
    gtest_discover_tests(datyredb_tests)
endif()
