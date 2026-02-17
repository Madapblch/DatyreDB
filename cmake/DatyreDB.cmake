# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  DatyreDB.cmake - Main configuration module                                  ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

include_guard(GLOBAL)

# ==============================================================================
# Standard CMake Modules
# ==============================================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)
include(FetchContent)
include(FeatureSummary)
include(CTest)

# ==============================================================================
# Project Modules (order matters!)
# ==============================================================================

include(DatyreDBOptions)
include(DatyreDBCompiler)
include(DatyreDBSanitizers)
include(DatyreDBDependencies)
include(DatyreDBTargets)  # Includes test/example/benchmark helpers

# ==============================================================================
# Global Settings (only if top-level project)
# ==============================================================================

if(DATYREDB_IS_TOP_LEVEL)
    # C++ Standard
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    
    # Output directories
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    
    # IDE folders
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
    
    # Export compile commands
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    
    # Colored output
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fdiagnostics-color=always)
    endif()
    
    # Hide symbols by default
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
    
    # Position Independent Code
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    
    # RPATH
    set(CMAKE_SKIP_BUILD_RPATH OFF)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
    
    if(APPLE)
        set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
    elseif(UNIX)
        set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    endif()
endif()

# ==============================================================================
# Configuration Headers
# ==============================================================================

file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/generated/datyredb")

if(EXISTS "${PROJECT_SOURCE_DIR}/src/datyredb/version.hpp.in")
    configure_file(
        "${PROJECT_SOURCE_DIR}/src/datyredb/version.hpp.in"
        "${PROJECT_BINARY_DIR}/generated/datyredb/version.hpp"
        @ONLY
    )
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/src/datyredb/config.hpp.in")
    configure_file(
        "${PROJECT_SOURCE_DIR}/src/datyredb/config.hpp.in"
        "${PROJECT_BINARY_DIR}/generated/datyredb/config.hpp"
        @ONLY
    )
endif()
