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
# CMake Settings
# ============================================================================
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Create a build directory.")
endif()

# ============================================================================
# Project Standards
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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
# Compiler Options
# ============================================================================

# Base flags for all builds
set(DATYREDB_CXX_FLAGS_BASE
    -Wall
    -Wextra
    -Wpedantic
    -Wcast-align
    -Wcast-qual
    -Wconversion
    -Wctor-dtor-privacy
    -Wdisabled-optimization
    -Wformat=2
    -Winit-self
    -Wmissing-declarations
    -Wmissing-include-dirs
    -Wold-style-cast
    -Woverloaded-virtual
    -Wredundant-decls
    -Wshadow
    -Wsign-conversion
    -Wsign-promo
    -Wstrict-overflow=5
    -Wswitch-default
    -Wundef
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    list(APPEND DATYREDB_CXX_FLAGS_BASE
        -Wlogical-op
        -Wnoexcept
        -Wstrict-null-sentinel
        -Wuseless-cast
        -Wzero-as-null-pointer-constant
    )
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.0)
        list(APPEND DATYREDB_CXX_FLAGS_BASE -Walloc-zero -Wduplicated-branches -Wduplicated-cond)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND DATYREDB_CXX_FLAGS_BASE
        -Weverything
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-padded
        -Wno-documentation-unknown-command
    )
endif()

# Build type specific flags
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG -DDATYREDB_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native -flto")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_COVERAGE "-O0 -g --coverage -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS_ASAN "-O1 -g -fsanitize=address -fno-omit-frame-pointer -DASAN_BUILD")
set(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fsanitize=thread -DTSAN_BUILD")

# Apply base flags
string(REPLACE ";" " " DATYREDB_CXX_FLAGS_STR "${DATYREDB_CXX_FLAGS_BASE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DATYREDB_CXX_FLAGS_STR}")

# ============================================================================
# Dependencies
# ============================================================================
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# Threading support
find_package(Threads REQUIRED)

# OpenSSL (optional, for encryption)
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    set(DATYREDB_HAVE_SSL TRUE)
endif()

# CURL for HTTP/HTTPS
find_package(CURL REQUIRED)
if(CURL_VERSION_STRING VERSION_LESS "7.60.0")
    message(WARNING "CURL version ${CURL_VERSION_STRING} is old, consider upgrading")
endif()

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# Google Test (for testing)
option(DATYREDB_BUILD_TESTS "Build unit tests" OFF)
if(DATYREDB_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

# Google Benchmark (for performance testing)
option(DATYREDB_BUILD_BENCHMARKS "Build benchmarks" OFF)
if(DATYREDB_BUILD_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
endif()

# spdlog for logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# Make dependencies available
FetchContent_MakeAvailable(nlohmann_json spdlog)

if(DATYREDB_BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
endif()

if(DATYREDB_BUILD_BENCHMARKS)
    FetchContent_MakeAvailable(benchmark)
endif()

# ============================================================================
# Version Information
# ============================================================================
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/version.hpp"
    @ONLY
)

# ============================================================================
# Source Files
# ============================================================================
set(DATYREDB_SOURCES
    src/main.cpp
    src/network/telegram_bot.cpp
)

set(DATYREDB_HEADERS
    src/network/telegram_bot.hpp
)

# ============================================================================
# Main Target
# ============================================================================
add_executable(${PROJECT_NAME} ${DATYREDB_SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        CURL::libcurl
        Threads::Threads
        $<$<BOOL:${DATYREDB_HAVE_SSL}>:OpenSSL::SSL>
        $<$<BOOL:${DATYREDB_HAVE_SSL}>:OpenSSL::Crypto>
        $<$<PLATFORM_ID:Linux>:rt>
        $<$<PLATFORM_ID:Linux>:dl>
        $<$<PLATFORM_ID:Linux>:m>
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        DATYREDB_VERSION="${PROJECT_VERSION}"
        DATYREDB_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        DATYREDB_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        DATYREDB_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        $<$<BOOL:${DATYREDB_HAVE_SSL}>:DATYREDB_HAVE_SSL>
        $<$<CONFIG:Debug>:DATYREDB_DEBUG>
        $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0601>
)

# Set RPATH for installed binaries
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH "$ORIGIN/../lib"
)

# ============================================================================
# Testing
# ============================================================================
if(DATYREDB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Benchmarks
# ============================================================================
if(DATYREDB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# ============================================================================
# Documentation
# ============================================================================
option(DATYREDB_BUILD_DOCS "Build documentation" OFF)
if(DATYREDB_BUILD_DOCS)
    find_package(Doxygen REQUIRED dot)
    if(DOXYGEN_FOUND)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
            "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
            @ONLY
        )
        add_custom_target(docs ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation"
            VERBATIM
        )
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT runtime
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config/
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/datyredb
    COMPONENT config
    FILES_MATCHING PATTERN "*.conf" PATTERN "*.json"
)

install(FILES README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT documentation
)

# ============================================================================
# Packaging
# ============================================================================
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Madapblch")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CONTACT "madapblch@github.com")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "libcurl4 (>= 7.60), libssl1.1")
set(CPACK_DEBIAN_PACKAGE_SECTION "database")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Databases")
set(CPACK_RPM_PACKAGE_REQUIRES "libcurl >= 7.60, openssl-libs")

set(CPACK_GENERATOR "TGZ;DEB;RPM")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
    "\\\\.git/"
    "\\\\.github/"
    "build/"
    "\\\\.vscode/"
    "\\\\.idea/"
    "\\\\.DS_Store"
)

include(CPack)

# ============================================================================
# Status Report
# ============================================================================
message(STATUS "")
message(STATUS "════════════════════════════════════════════════════════════")
message(STATUS " ${PROJECT_NAME} v${PROJECT_VERSION} Configuration Summary")
message(STATUS "════════════════════════════════════════════════════════════")
message(STATUS " Build Configuration:")
message(STATUS "   Build Type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "   Install Prefix:       ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS " Compiler:")
message(STATUS "   C++ Compiler:         ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "   C++ Standard:         C++${CMAKE_CXX_STANDARD}")
message(STATUS "   Compile Flags:        ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
message(STATUS "")
message(STATUS " Dependencies:")
message(STATUS "   nlohmann/json:        ✓")
message(STATUS "   spdlog:               ✓")
message(STATUS "   CURL:                 ${CURL_VERSION_STRING}")
message(STATUS "   OpenSSL:              ${OPENSSL_VERSION}")
message(STATUS "   Threads:              ✓")
message(STATUS "")
message(STATUS " Optional Components:")
message(STATUS "   Build Tests:          ${DATYREDB_BUILD_TESTS}")
message(STATUS "   Build Benchmarks:     ${DATYREDB_BUILD_BENCHMARKS}")
message(STATUS "   Build Documentation:  ${DATYREDB_BUILD_DOCS}")
message(STATUS "════════════════════════════════════════════════════════════")
message(STATUS "")
