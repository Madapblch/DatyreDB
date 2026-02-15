cmake_minimum_required(VERSION 3.16)

# ============================================================================
# Project Definition
# ============================================================================
project(DatyreDB 
    VERSION 1.0.0
    DESCRIPTION "High-Performance Database Management System"
    HOMEPAGE_URL "https://github.com/Madapblch/DatyreDB"
    LANGUAGES CXX C
)

# ============================================================================
# CMake Modules and Policies
# ============================================================================
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Modern CMake policies
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)  # ExternalProject download timestamp
endif()
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
endif()

# ============================================================================
# Project Standards
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR 
        "In-source builds are not allowed.\n"
        "Please create a separate build directory:\n"
        "  mkdir build && cd build && cmake .."
    )
endif()

# ============================================================================
# Build Type Configuration
# ============================================================================
set(DATYREDB_BUILD_TYPES "Debug" "Release" "RelWithDebInfo" "MinSizeRel" "Coverage" "ASAN" "TSAN")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${DATYREDB_BUILD_TYPES})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not specified: defaulting to Release")
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)

# ============================================================================
# Compiler Detection and Flags
# ============================================================================

# Detect compiler
set(DATYREDB_COMPILER_GCC FALSE)
set(DATYREDB_COMPILER_CLANG FALSE)
set(DATYREDB_COMPILER_MSVC FALSE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(DATYREDB_COMPILER_GCC TRUE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(DATYREDB_COMPILER_CLANG TRUE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(DATYREDB_COMPILER_MSVC TRUE)
endif()

# Base warning flags
if(DATYREDB_COMPILER_GCC OR DATYREDB_COMPILER_CLANG)
    set(DATYREDB_WARNING_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wcast-align
        -Wcast-qual
        -Wconversion
        -Wformat=2
        -Wmissing-include-dirs
        -Wold-style-cast
        -Woverloaded-virtual
        -Wshadow
        -Wsign-conversion
        -Wundef
        -Wno-unused-parameter
    )
    
    if(DATYREDB_COMPILER_GCC)
        list(APPEND DATYREDB_WARNING_FLAGS
            -Wlogical-op
            -Wuseless-cast
            -Wduplicated-cond
            -Wduplicated-branches
        )
    endif()
elseif(DATYREDB_COMPILER_MSVC)
    set(DATYREDB_WARNING_FLAGS
        /W4
        /WX-
        /permissive-
        /Zc:__cplusplus
    )
endif()

# Build type specific flags
if(DATYREDB_COMPILER_GCC OR DATYREDB_COMPILER_CLANG)
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native -flto")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG -march=native")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG -flto")
    set(CMAKE_CXX_FLAGS_COVERAGE "-O0 -g --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS_ASAN "-O1 -g -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fsanitize=thread")
elseif(DATYREDB_COMPILER_MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /MDd /Zi /RTC1 /DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MD /DNDEBUG")
endif()

# ============================================================================
# Options
# ============================================================================
option(DATYREDB_BUILD_TESTS "Build unit tests" OFF)
option(DATYREDB_BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(DATYREDB_BUILD_DOCS "Build documentation" OFF)
option(DATYREDB_ENABLE_PROFILING "Enable profiling instrumentation" OFF)
option(DATYREDB_ENABLE_LTO "Enable Link Time Optimization" ON)
option(DATYREDB_USE_JEMALLOC "Use jemalloc for memory allocation" OFF)
option(DATYREDB_STATIC_LINK "Static link dependencies" OFF)

# ============================================================================
# Version Information
# ============================================================================
# Generate version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/version.hpp"
    @ONLY
)

# ============================================================================
# Dependencies
# ============================================================================
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# Threading
find_package(Threads REQUIRED)

# OpenSSL (optional)
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    set(DATYREDB_HAVE_SSL TRUE)
endif()

# CURL
find_package(CURL REQUIRED)
if(CURL_VERSION_STRING VERSION_LESS "7.60.0")
    message(WARNING "CURL version ${CURL_VERSION_STRING} is old. Consider upgrading to 7.60.0+")
endif()

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# spdlog for logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# CLI11 for command line parsing
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.3.2
    GIT_SHALLOW TRUE
)

# Make dependencies available
FetchContent_MakeAvailable(nlohmann_json spdlog CLI11)

# Google Test (optional)
if(DATYREDB_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

# Google Benchmark (optional)
if(DATYREDB_BUILD_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
        GIT_SHALLOW TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

# jemalloc (optional)
if(DATYREDB_USE_JEMALLOC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JEMALLOC jemalloc)
    if(JEMALLOC_FOUND)
        message(STATUS "Using jemalloc: ${JEMALLOC_VERSION}")
    else()
        message(WARNING "jemalloc requested but not found")
        set(DATYREDB_USE_JEMALLOC OFF)
    endif()
endif()

# ============================================================================
# Source Files
# ============================================================================

# Core sources
set(CORE_SOURCES
    src/core/database.cpp
    src/core/storage_engine.cpp
    src/core/query_executor.cpp
)

# Network sources
set(NETWORK_SOURCES
    src/network/telegram_bot.cpp
)

# Utility sources
set(UTILITY_SOURCES
    src/utils/logger.cpp
)

# All sources
set(DATYREDB_SOURCES
    src/main.cpp
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${UTILITY_SOURCES}
)

# ============================================================================
# Main Target
# ============================================================================
add_executable(${PROJECT_NAME} ${DATYREDB_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Compile options
target_compile_options(${PROJECT_NAME} PRIVATE ${DATYREDB_WARNING_FLAGS})

# Link libraries
set(DATYREDB_LINK_LIBRARIES
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    CLI11::CLI11
    CURL::libcurl
    Threads::Threads
)

if(DATYREDB_HAVE_SSL)
    list(APPEND DATYREDB_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
endif()

if(DATYREDB_USE_JEMALLOC AND JEMALLOC_FOUND)
    list(APPEND DATYREDB_LINK_LIBRARIES ${JEMALLOC_LIBRARIES})
endif()

if(UNIX AND NOT APPLE)
    list(APPEND DATYREDB_LINK_LIBRARIES rt dl m)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${DATYREDB_LINK_LIBRARIES})

# Compile definitions
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        DATYREDB_VERSION="${PROJECT_VERSION}"
        DATYREDB_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        DATYREDB_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        DATYREDB_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        $<$<BOOL:${DATYREDB_HAVE_SSL}>:DATYREDB_HAVE_SSL>
        $<$<BOOL:${DATYREDB_USE_JEMALLOC}>:DATYREDB_USE_JEMALLOC>
        $<$<CONFIG:Debug>:DATYREDB_DEBUG>
)

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Link Time Optimization
if(DATYREDB_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT error)
    if(ipo_supported)
        set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "Link Time Optimization enabled")
    endif()
endif()

# ============================================================================
# Testing
# ============================================================================
if(DATYREDB_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ============================================================================
# Benchmarks
# ============================================================================
if(DATYREDB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)

# Install main executable
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install config files (if directory exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/config/"
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/datyredb
        FILES_MATCHING
            PATTERN "*.conf"
            PATTERN "*.json"
            PATTERN "*.yaml"
    )
endif()

# Install documentation (if files exist)
foreach(doc_file README.md LICENSE CHANGELOG.md)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${doc_file}")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${doc_file}"
            DESTINATION ${CMAKE_INSTALL_DOCDIR}
        )
    endif()
endforeach()

# ============================================================================
# CPack Configuration
# ============================================================================
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Madapblch")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_CONTACT "madapblch@github.com")

if(UNIX)
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
endif()

include(CPack)

# ============================================================================
# Status Report
# ============================================================================
message(STATUS "")
message(STATUS "════════════════════════════════════════════════════════════")
message(STATUS " ${PROJECT_NAME} v${PROJECT_VERSION} Configuration")
message(STATUS "════════════════════════════════════════════════════════════")
message(STATUS " Build:")
message(STATUS "   Type:              ${CMAKE_BUILD_TYPE}")
message(STATUS "   C++ Standard:      C++${CMAKE_CXX_STANDARD}")
message(STATUS "   Compiler:          ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " ")
message(STATUS " Dependencies:")
message(STATUS "   CURL:              ${CURL_VERSION_STRING}")
message(STATUS "   OpenSSL:           ${OPENSSL_VERSION}")
message(STATUS "   nlohmann/json:     ✓ (fetched)")
message(STATUS "   spdlog:            ✓ (fetched)")
message(STATUS "   CLI11:             ✓ (fetched)")
if(DATYREDB_USE_JEMALLOC)
message(STATUS "   jemalloc:          ${JEMALLOC_VERSION}")
endif()
message(STATUS " ")
message(STATUS " Options:")
message(STATUS "   Tests:             ${DATYREDB_BUILD_TESTS}")
message(STATUS "   Benchmarks:        ${DATYREDB_BUILD_BENCHMARKS}")
message(STATUS "   Documentation:     ${DATYREDB_BUILD_DOCS}")
message(STATUS "   LTO:               ${DATYREDB_ENABLE_LTO}")
message(STATUS "   Profiling:         ${DATYREDB_ENABLE_PROFILING}")
message(STATUS "════════════════════════════════════════════════════════════")
message(STATUS "")
