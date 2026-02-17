# ╔══════��═══════════════════════════════════════════════════════════════════════╗
# ║  DatyreDBDependencies.cmake - External dependencies                          ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

include_guard(GLOBAL)

# ==============================================================================
# FetchContent Configuration
# ==============================================================================

set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ==============================================================================
# Threads (always required)
# ==============================================================================

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ==============================================================================
# fmt
# ==============================================================================

if(DATYREDB_USE_EXTERNAL_FMT)
    find_package(fmt REQUIRED CONFIG)
    message(STATUS "Using external fmt: ${fmt_VERSION}")
else()
    find_package(fmt QUIET CONFIG)
    
    if(fmt_FOUND)
        message(STATUS "Found system fmt: ${fmt_VERSION}")
    else()
        message(STATUS "Downloading fmt...")
        
        FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG        10.2.1
            GIT_SHALLOW    TRUE
            SYSTEM
            FIND_PACKAGE_ARGS CONFIG
        )
        
        set(FMT_DOC OFF CACHE INTERNAL "")
        set(FMT_TEST OFF CACHE INTERNAL "")
        set(FMT_INSTALL OFF CACHE INTERNAL "")
        
        FetchContent_MakeAvailable(fmt)
    endif()
endif()

# ==============================================================================
# spdlog (optional)
# ==============================================================================

if(DATYREDB_USE_EXTERNAL_SPDLOG)
    find_package(spdlog REQUIRED CONFIG)
    set(DATYREDB_HAS_SPDLOG ON CACHE INTERNAL "")
    message(STATUS "Using external spdlog: ${spdlog_VERSION}")
else()
    find_package(spdlog QUIET CONFIG)
    
    if(spdlog_FOUND)
        set(DATYREDB_HAS_SPDLOG ON CACHE INTERNAL "")
        message(STATUS "Found system spdlog: ${spdlog_VERSION}")
    else()
        set(DATYREDB_HAS_SPDLOG OFF CACHE INTERNAL "")
        message(STATUS "spdlog not found - using simple logging")
    endif()
endif()

# ==============================================================================
# GoogleTest (for tests)
# ==============================================================================

if(DATYREDB_BUILD_TESTS)
    if(DATYREDB_USE_EXTERNAL_GTEST)
        find_package(GTest REQUIRED CONFIG)
        message(STATUS "Using external GTest: ${GTest_VERSION}")
    else()
        find_package(GTest QUIET CONFIG)
        
        if(GTest_FOUND)
            message(STATUS "Found system GTest: ${GTest_VERSION}")
        else()
            message(STATUS "Downloading GoogleTest...")
            
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG        v1.14.0
                GIT_SHALLOW    TRUE
                SYSTEM
                FIND_PACKAGE_ARGS CONFIG NAMES GTest
            )
            
            set(BUILD_GMOCK OFF CACHE INTERNAL "")
            set(INSTALL_GTEST OFF CACHE INTERNAL "")
            set(gtest_force_shared_crt ON CACHE INTERNAL "")
            
            FetchContent_MakeAvailable(googletest)
        endif()
    endif()
endif()

# ==============================================================================
# Google Benchmark (for benchmarks)
# ==============================================================================

if(DATYREDB_BUILD_BENCHMARKS)
    if(DATYREDB_USE_EXTERNAL_BENCHMARK)
        find_package(benchmark REQUIRED CONFIG)
        message(STATUS "Using external benchmark: ${benchmark_VERSION}")
    else()
        find_package(benchmark QUIET CONFIG)
        
        if(benchmark_FOUND)
            message(STATUS "Found system benchmark: ${benchmark_VERSION}")
        else()
            message(STATUS "Downloading Google Benchmark...")
            
            FetchContent_Declare(
                benchmark
                GIT_REPOSITORY https://github.com/google/benchmark.git
                GIT_TAG        v1.8.3
                GIT_SHALLOW    TRUE
                SYSTEM
            )
            
            set(BENCHMARK_ENABLE_TESTING OFF CACHE INTERNAL "")
            set(BENCHMARK_ENABLE_INSTALL OFF CACHE INTERNAL "")
            set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE INTERNAL "")
            
            FetchContent_MakeAvailable(benchmark)
        endif()
    endif()
endif()
