# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  DatyreDBOptions.cmake - Build options                                       ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

include_guard(GLOBAL)

# ==============================================================================
# Build Type
# ==============================================================================

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type specified, defaulting to RelWithDebInfo")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")

# ==============================================================================
# Component Options
# ==============================================================================

option(DATYREDB_BUILD_TESTS        "Build test suite"                   "${DATYREDB_IS_TOP_LEVEL}")
option(DATYREDB_BUILD_BENCHMARKS   "Build benchmarks"                   OFF)
option(DATYREDB_BUILD_EXAMPLES     "Build example programs"             "${DATYREDB_IS_TOP_LEVEL}")
option(DATYREDB_BUILD_DOCS         "Build documentation"                OFF)
option(DATYREDB_BUILD_SHARED_LIBS  "Build shared libraries"             "${BUILD_SHARED_LIBS}")

# ==============================================================================
# Installation Options
# ==============================================================================

option(DATYREDB_ENABLE_INSTALL     "Enable installation targets"        "${DATYREDB_IS_TOP_LEVEL}")

# ==============================================================================
# Development Options
# ==============================================================================

option(DATYREDB_ENABLE_WARNINGS    "Enable compiler warnings"           "${DATYREDB_IS_TOP_LEVEL}")
option(DATYREDB_ENABLE_WERROR      "Treat warnings as errors"           OFF)
option(DATYREDB_ENABLE_PEDANTIC    "Enable pedantic warnings"           OFF)

# ==============================================================================
# Sanitizer Options (mutually exclusive)
# ==============================================================================

option(DATYREDB_ENABLE_ASAN        "Enable AddressSanitizer"            OFF)
option(DATYREDB_ENABLE_TSAN        "Enable ThreadSanitizer"             OFF)
option(DATYREDB_ENABLE_UBSAN       "Enable UndefinedBehaviorSanitizer"  OFF)
option(DATYREDB_ENABLE_MSAN        "Enable MemorySanitizer (Clang)"     OFF)

# Validate sanitizer options
set(_sanitizer_count 0)
foreach(_san IN ITEMS ASAN TSAN MSAN)
    if(DATYREDB_ENABLE_${_san})
        math(EXPR _sanitizer_count "${_sanitizer_count} + 1")
    endif()
endforeach()

if(_sanitizer_count GREATER 1)
    message(FATAL_ERROR 
        "Only one memory sanitizer can be enabled at a time.\n"
        "ASAN=${DATYREDB_ENABLE_ASAN}, TSAN=${DATYREDB_ENABLE_TSAN}, MSAN=${DATYREDB_ENABLE_MSAN}"
    )
endif()
unset(_sanitizer_count)

# ==============================================================================
# Optimization Options
# ==============================================================================

option(DATYREDB_ENABLE_LTO         "Enable Link Time Optimization"      OFF)
option(DATYREDB_ENABLE_NATIVE      "Enable native CPU optimizations"    OFF)
option(DATYREDB_ENABLE_COVERAGE    "Enable code coverage"               OFF)

# ==============================================================================
# Feature Options
# ==============================================================================

option(DATYREDB_ENABLE_LOGGING     "Enable logging"                     ON)
option(DATYREDB_ENABLE_ASSERTIONS  "Enable assertions in release"       OFF)
option(DATYREDB_ENABLE_TRACING     "Enable detailed tracing"            OFF)

# ==============================================================================
# Dependency Options
# ==============================================================================

option(DATYREDB_USE_EXTERNAL_FMT       "Use system/vcpkg fmt"           OFF)
option(DATYREDB_USE_EXTERNAL_SPDLOG    "Use system/vcpkg spdlog"        OFF)
option(DATYREDB_USE_EXTERNAL_GTEST     "Use system/vcpkg GoogleTest"    OFF)
option(DATYREDB_USE_EXTERNAL_BENCHMARK "Use system/vcpkg benchmark"     OFF)

# ==============================================================================
# Feature Summary Registration
# ==============================================================================

add_feature_info("Tests" DATYREDB_BUILD_TESTS "Build unit and integration tests")
add_feature_info("Benchmarks" DATYREDB_BUILD_BENCHMARKS "Build performance benchmarks")
add_feature_info("Examples" DATYREDB_BUILD_EXAMPLES "Build example programs")
add_feature_info("Documentation" DATYREDB_BUILD_DOCS "Build Doxygen documentation")
add_feature_info("AddressSanitizer" DATYREDB_ENABLE_ASAN "Memory error detection")
add_feature_info("ThreadSanitizer" DATYREDB_ENABLE_TSAN "Data race detection")
add_feature_info("UBSanitizer" DATYREDB_ENABLE_UBSAN "Undefined behavior detection")
add_feature_info("Coverage" DATYREDB_ENABLE_COVERAGE "Code coverage reporting")
add_feature_info("LTO" DATYREDB_ENABLE_LTO "Link Time Optimization")
