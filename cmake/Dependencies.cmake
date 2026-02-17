# ==============================================================================
# Dependencies.cmake - External dependencies
# ==============================================================================

include_guard(GLOBAL)

# ==============================================================================
# Threads
# ==============================================================================

find_package(Threads REQUIRED)

# ==============================================================================
# fmt library
# ==============================================================================

if(DATYREDB_USE_SYSTEM_DEPS)
    find_package(fmt REQUIRED)
else()
    find_package(fmt QUIET)
    
    if(NOT fmt_FOUND)
        message(STATUS "fmt not found - downloading via FetchContent")
        
        FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG        10.2.1
            GIT_SHALLOW    TRUE
            GIT_PROGRESS   TRUE
        )
        
        # Disable tests and install for fmt
        set(FMT_DOC OFF CACHE BOOL "" FORCE)
        set(FMT_TEST OFF CACHE BOOL "" FORCE)
        set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
        
        FetchContent_MakeAvailable(fmt)
    endif()
endif()

# ==============================================================================
# spdlog (optional)
# ==============================================================================

find_package(spdlog QUIET)

if(spdlog_FOUND)
    message(STATUS "Found spdlog - using advanced logging")
    set(DATYREDB_HAS_SPDLOG TRUE CACHE INTERNAL "")
else()
    message(STATUS "spdlog not found - using simple logging")
    set(DATYREDB_HAS_SPDLOG FALSE CACHE INTERNAL "")
endif()
