# ==============================================================================
# DatyreDBBuildOptions.cmake - Build options
# ==============================================================================

include_guard(GLOBAL)

# Helper macro for dependent options
include(CMakeDependentOption)

# ==============================================================================
# Build type
# ==============================================================================

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# ==============================================================================
# Main options
# ==============================================================================

option(DATYREDB_BUILD_TESTS "Build tests" ON)
option(DATYREDB_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(DATYREDB_BUILD_EXAMPLES "Build examples" ON)
option(DATYREDB_BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(DATYREDB_ENABLE_INSTALL "Enable installation" ON)

# ==============================================================================
# Developer options
# ==============================================================================

option(DATYREDB_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(DATYREDB_ENABLE_WERROR "Treat warnings as errors" OFF)

# ==============================================================================
# Debug options (mutually exclusive sanitizers)
# ==============================================================================

option(DATYREDB_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(DATYREDB_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(DATYREDB_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(DATYREDB_ENABLE_MSAN "Enable MemorySanitizer" OFF)
option(DATYREDB_ENABLE_COVERAGE "Enable code coverage" OFF)

# Sanitizer conflict checks
set(_sanitizer_count 0)
foreach(_san ASAN TSAN MSAN)
    if(DATYREDB_ENABLE_${_san})
        math(EXPR _sanitizer_count "${_sanitizer_count} + 1")
    endif()
endforeach()
if(_sanitizer_count GREATER 1)
    message(FATAL_ERROR "Only one of ASAN, TSAN, MSAN can be enabled at a time")
endif()
unset(_sanitizer_count)

# ==============================================================================
# Optimization options
# ==============================================================================

option(DATYREDB_ENABLE_LTO "Enable Link Time Optimization" OFF)
option(DATYREDB_ENABLE_NATIVE "Enable native CPU optimizations" OFF)

# ==============================================================================
# Feature options
# ==============================================================================

option(DATYREDB_ENABLE_LOGGING "Enable logging" ON)

# ==============================================================================
# Dependency options
# ==============================================================================

option(DATYREDB_USE_EXTERNAL_FMT "Use external fmt library" OFF)
option(DATYREDB_USE_EXTERNAL_GTEST "Use external GoogleTest" OFF)
