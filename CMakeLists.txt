# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║                                                                              ║
# ║                              DatyreDB                                        ║
# ║                                                                              ║
# ║          High-Performance Embedded Database Engine                           ║
# ║                                                                              ║
# ║  Copyright (c) 2024 DatyreDB Authors                                        ║
# ║  SPDX-License-Identifier: MIT                                               ║
# ║                                                                              ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

# ==============================================================================
# CMake Requirements
# ==============================================================================

cmake_minimum_required(VERSION 3.21...3.29)

# Policies for consistent behavior
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
cmake_policy(SET CMP0079 NEW)  # target_link_libraries from other directories
cmake_policy(SET CMP0091 NEW)  # MSVC runtime library selection
cmake_policy(SET CMP0135 NEW)  # FetchContent download timestamp

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)  # find_package uses upper-case <PACKAGENAME>_ROOT
endif()

# ==============================================================================
# Prevent In-Source Builds
# ==============================================================================

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "╔════════════════════════════════════════════════════════════════╗\n"
        "║  ERROR: In-source builds are not allowed!                      ║\n"
        "║                                                                ║\n"
        "║  Please create a separate build directory:                     ║\n"
        "║    cmake -B build -S .                                         ║\n"
        "║    cmake --build build                                         ║\n"
        "║                                                                ║\n"
        "║  Or use presets:                                               ║\n"
        "║    cmake --preset=dev                                          ║\n"
        "║    cmake --build --preset=dev                                  ║\n"
        "╚════════════════════════════════════════════════════════════════╝"
    )
endif()

# ==============================================================================
# Version
# ==============================================================================

# Read version from file for single source of truth
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _version_content)
string(STRIP "${_version_content}" _version_content)
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${_version_content}")
set(DATYREDB_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(DATYREDB_VERSION_MINOR "${CMAKE_MATCH_2}")
set(DATYREDB_VERSION_PATCH "${CMAKE_MATCH_3}")
set(DATYREDB_VERSION "${DATYREDB_VERSION_MAJOR}.${DATYREDB_VERSION_MINOR}.${DATYREDB_VERSION_PATCH}")

# Git information (for development builds)
find_package(Git QUIET)
if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE DATYREDB_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --always --dirty
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE DATYREDB_GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(DATYREDB_GIT_HASH "unknown")
    set(DATYREDB_GIT_DESCRIBE "${DATYREDB_VERSION}")
endif()

# ==============================================================================
# Project Declaration
# ==============================================================================

project(DatyreDB
    VERSION "${DATYREDB_VERSION}"
    DESCRIPTION "High-Performance Embedded Database Engine"
    HOMEPAGE_URL "https://github.com/Madapblch/DatyreDB"
    LANGUAGES CXX
)

# Determine if this is the top-level project
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}" DATYREDB_IS_TOP_LEVEL)

# ==============================================================================
# CMake Module Path
# ==============================================================================

list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

# ==============================================================================
# Include Main Configuration Module
# ==============================================================================

include(DatyreDB)

# ==============================================================================
# Subdirectories
# ==============================================================================

add_subdirectory(src)

if(DATYREDB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(DATYREDB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(DATYREDB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(DATYREDB_BUILD_DOCS)
    add_subdirectory(docs)
endif()

# ==============================================================================
# Installation & Packaging
# ==============================================================================

if(DATYREDB_ENABLE_INSTALL AND DATYREDB_IS_TOP_LEVEL)
    include(DatyreDBInstall)
    include(DatyreDBPackaging)
endif()

# ==============================================================================
# Summary
# ==============================================================================

if(DATYREDB_IS_TOP_LEVEL)
    include(FeatureSummary)
    feature_summary(WHAT ALL)
    
    message(STATUS "")
    message(STATUS "╔══════════════════════════════════════════════════════════════════╗")
    message(STATUS "║                    DatyreDB ${DATYREDB_VERSION} (${DATYREDB_GIT_HASH})")
    message(STATUS "╠══════════════════════════════════════════════════════════════════╣")
    message(STATUS "║  Platform:     ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "║  Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "║  Build type:   ${CMAKE_BUILD_TYPE}")
    message(STATUS "║  C++ Standard: ${CMAKE_CXX_STANDARD}")
    message(STATUS "╠══════════════════════════════════════════════════════════════════╣")
    message(STATUS "║  Tests:        ${DATYREDB_BUILD_TESTS}")
    message(STATUS "║  Benchmarks:   ${DATYREDB_BUILD_BENCHMARKS}")
    message(STATUS "║  Examples:     ${DATYREDB_BUILD_EXAMPLES}")
    message(STATUS "║  Docs:         ${DATYREDB_BUILD_DOCS}")
    message(STATUS "║  Install:      ${DATYREDB_ENABLE_INSTALL}")
    message(STATUS "╠══════════════════════════════════════════════════════════════════╣")
    if(DATYREDB_ENABLE_ASAN OR DATYREDB_ENABLE_TSAN OR DATYREDB_ENABLE_UBSAN OR DATYREDB_ENABLE_MSAN)
        message(STATUS "║  Sanitizers:")
        if(DATYREDB_ENABLE_ASAN)
            message(STATUS "║    - AddressSanitizer")
        endif()
        if(DATYREDB_ENABLE_TSAN)
            message(STATUS "║    - ThreadSanitizer")
        endif()
        if(DATYREDB_ENABLE_UBSAN)
            message(STATUS "║    - UndefinedBehaviorSanitizer")
        endif()
        if(DATYREDB_ENABLE_MSAN)
            message(STATUS "║    - MemorySanitizer")
        endif()
    endif()
    message(STATUS "╚══════════════════════════════════════════════════════════════════╝")
    message(STATUS "")
endif()
