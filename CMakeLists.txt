cmake_minimum_required(VERSION 3.16)
project(DatyreDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /W4 /MDd")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG /MD")
endif()

# Find packages (optional for network features)
find_package(Threads REQUIRED)
find_package(CURL QUIET)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Collect existing source files
set(DATYREDB_SOURCES "")

# SQL Parser files (эти у тебя точно есть)
if(EXISTS "${PROJECT_SOURCE_DIR}/src/sql/lexer.cpp")
    list(APPEND DATYREDB_SOURCES ${PROJECT_SOURCE_DIR}/src/sql/lexer.cpp)
endif()
if(EXISTS "${PROJECT_SOURCE_DIR}/src/sql/parser.cpp")
    list(APPEND DATYREDB_SOURCES ${PROJECT_SOURCE_DIR}/src/sql/parser.cpp)
endif()
if(EXISTS "${PROJECT_SOURCE_DIR}/src/sql/ast.cpp")
    list(APPEND DATYREDB_SOURCES ${PROJECT_SOURCE_DIR}/src/sql/ast.cpp)
endif()

# Storage files (если есть)
if(EXISTS "${PROJECT_SOURCE_DIR}/src/storage/btree.cpp")
    list(APPEND DATYREDB_SOURCES ${PROJECT_SOURCE_DIR}/src/storage/btree.cpp)
endif()

# Network files (если ты добавил из моего ответа)
if(EXISTS "${PROJECT_SOURCE_DIR}/src/network/server.cpp")
    list(APPEND DATYREDB_SOURCES ${PROJECT_SOURCE_DIR}/src/network/server.cpp)
endif()
if(EXISTS "${PROJECT_SOURCE_DIR}/src/network/connection.cpp")
    list(APPEND DATYREDB_SOURCES ${PROJECT_SOURCE_DIR}/src/network/connection.cpp)
endif()
if(EXISTS "${PROJECT_SOURCE_DIR}/src/network/session.cpp")
    list(APPEND DATYREDB_SOURCES ${PROJECT_SOURCE_DIR}/src/network/session.cpp)
endif()
if(EXISTS "${PROJECT_SOURCE_DIR}/src/network/http_server.cpp")
    list(APPEND DATYREDB_SOURCES ${PROJECT_SOURCE_DIR}/src/network/http_server.cpp)
endif()
if(EXISTS "${PROJECT_SOURCE_DIR}/src/network/telegram_bot.cpp")
    list(APPEND DATYREDB_SOURCES ${PROJECT_SOURCE_DIR}/src/network/telegram_bot.cpp)
endif()

# Fallback source files list (минимальный набор)
if(NOT DATYREDB_SOURCES)
    message(WARNING "No source files found, using minimal configuration")
    file(GLOB_RECURSE DATYREDB_SOURCES 
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
    )
    # Remove main.cpp from library sources
    list(REMOVE_ITEM DATYREDB_SOURCES "${PROJECT_SOURCE_DIR}/src/main.cpp")
endif()

# Main executable
add_executable(DatyreDB 
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    ${DATYREDB_SOURCES}
)

# Link libraries
target_link_libraries(DatyreDB 
    PRIVATE 
        Threads::Threads
)

# Link CURL only if found (for Telegram bot)
if(CURL_FOUND)
    target_link_libraries(DatyreDB PRIVATE CURL::libcurl)
    target_compile_definitions(DatyreDB PRIVATE HAS_CURL=1)
    message(STATUS "CURL found - Telegram bot support enabled")
else()
    message(STATUS "CURL not found - Telegram bot support disabled")
    message(STATUS "To enable Telegram bot, install libcurl:")
    message(STATUS "  Ubuntu/Debian: sudo apt install libcurl4-openssl-dev")
    message(STATUS "  macOS: brew install curl")
endif()

# Optional: Google Test (если хочешь тесты)
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS AND EXISTS "${PROJECT_SOURCE_DIR}/third_party/googletest")
    enable_testing()
    add_subdirectory(third_party/googletest)
    
    file(GLOB_RECURSE TEST_SOURCES "${PROJECT_SOURCE_DIR}/tests/*.cpp")
    
    if(TEST_SOURCES)
        add_executable(datyredb_tests ${TEST_SOURCES} ${DATYREDB_SOURCES})
        target_link_libraries(datyredb_tests 
            PRIVATE 
                gtest_main
                Threads::Threads
        )
        
        if(CURL_FOUND)
            target_link_libraries(datyredb_tests PRIVATE CURL::libcurl)
        endif()
        
        include(GoogleTest)
        gtest_discover_tests(datyredb_tests)
    endif()
endif()

# Installation
install(TARGETS DatyreDB DESTINATION bin)

# Status messages
message(STATUS "========================================")
message(STATUS "DatyreDB Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
if(BUILD_TESTS)
    message(STATUS "  Tests: Enabled")
else()
    message(STATUS "  Tests: Disabled")
endif()
if(CURL_FOUND)
    message(STATUS "  Network: Full (with Telegram bot)")
else()
    message(STATUS "  Network: Basic (no Telegram bot)")
endif()
message(STATUS "========================================")
