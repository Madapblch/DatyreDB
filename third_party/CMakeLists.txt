# ==============================================================================
# third_party/CMakeLists.txt - External dependencies
# ==============================================================================

include(FetchContent)

# ==============================================================================
# FetchContent settings
# ==============================================================================

set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ==============================================================================
# fmt
# ==============================================================================

if(DATYREDB_USE_EXTERNAL_FMT)
    find_package(fmt REQUIRED CONFIG)
else()
    find_package(fmt QUIET CONFIG)
    
    if(NOT fmt_FOUND)
        message(STATUS "Fetching fmt...")
        
        FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG        10.2.1
            GIT_SHALLOW    TRUE
        )
        
        set(FMT_DOC OFF CACHE INTERNAL "")
        set(FMT_TEST OFF CACHE INTERNAL "")
        set(FMT_INSTALL OFF CACHE INTERNAL "")
        set(FMT_OS OFF CACHE INTERNAL "")
        
        FetchContent_MakeAvailable(fmt)
        
        # Mark as system headers to suppress warnings
        get_target_property(_fmt_include_dirs fmt-header-only INTERFACE_INCLUDE_DIRECTORIES)
        if(_fmt_include_dirs)
            set_target_properties(fmt-header-only PROPERTIES
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_fmt_include_dirs}")
        endif()
    endif()
endif()

# ==============================================================================
# GoogleTest (only if building tests)
# ==============================================================================

if(DATYREDB_BUILD_TESTS)
    if(DATYREDB_USE_EXTERNAL_GTEST)
        find_package(GTest REQUIRED CONFIG)
    else()
        find_package(GTest QUIET CONFIG)
        
        if(NOT GTest_FOUND)
            message(STATUS "Fetching GoogleTest...")
            
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG        v1.14.0
                GIT_SHALLOW    TRUE
            )
            
            set(BUILD_GMOCK OFF CACHE INTERNAL "")
            set(INSTALL_GTEST OFF CACHE INTERNAL "")
            set(gtest_force_shared_crt ON CACHE INTERNAL "")
            
            FetchContent_MakeAvailable(googletest)
        endif()
    endif()
endif()

# ==============================================================================
# Google Benchmark (only if building benchmarks)
# ==============================================================================

if(DATYREDB_BUILD_BENCHMARKS)
    find_package(benchmark QUIET CONFIG)
    
    if(NOT benchmark_FOUND)
        message(STATUS "Fetching Google Benchmark...")
        
        FetchContent_Declare(
            benchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG        v1.8.3
            GIT_SHALLOW    TRUE
        )
        
        set(BENCHMARK_ENABLE_TESTING OFF CACHE INTERNAL "")
        set(BENCHMARK_ENABLE_INSTALL OFF CACHE INTERNAL "")
        set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE INTERNAL "")
        
        FetchContent_MakeAvailable(benchmark)
    endif()
endif()
