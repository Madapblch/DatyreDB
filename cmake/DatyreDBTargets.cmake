# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  DatyreDBTargets.cmake - Target creation and configuration                   ║
# ║                                                                              ║
# ║  This module provides functions to create targets with consistent            ║
# ║  compiler options, include paths, and dependencies.                          ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

include_guard(GLOBAL)

# ==============================================================================
# Private: Compiler Options (internal use only)
# ==============================================================================

# Collect all compiler flags into lists (set by DatyreDBCompiler.cmake)
set(DATYREDB_INTERNAL_COMPILE_OPTIONS
    ${DATYREDB_CXX_FLAGS}
    ${DATYREDB_SANITIZER_FLAGS}
)

set(DATYREDB_INTERNAL_COMPILE_OPTIONS_DEBUG
    ${DATYREDB_CXX_FLAGS_DEBUG}
)

set(DATYREDB_INTERNAL_COMPILE_OPTIONS_RELEASE
    ${DATYREDB_CXX_FLAGS_RELEASE}
)

set(DATYREDB_INTERNAL_LINK_OPTIONS
    ${DATYREDB_LINK_FLAGS}
    ${DATYREDB_SANITIZER_LINK_FLAGS}
)

set(DATYREDB_INTERNAL_LINK_OPTIONS_RELEASE
    ${DATYREDB_LINK_FLAGS_RELEASE}
)

# ==============================================================================
# Private: Apply compiler options to target
# ==============================================================================

function(_datyredb_apply_compile_options TARGET_NAME VISIBILITY)
    # Compiler options
    target_compile_options(${TARGET_NAME} ${VISIBILITY}
        ${DATYREDB_INTERNAL_COMPILE_OPTIONS}
        $<$<CONFIG:Debug>:${DATYREDB_INTERNAL_COMPILE_OPTIONS_DEBUG}>
        $<$<CONFIG:Release>:${DATYREDB_INTERNAL_COMPILE_OPTIONS_RELEASE}>
        $<$<CONFIG:RelWithDebInfo>:${DATYREDB_INTERNAL_COMPILE_OPTIONS_RELEASE}>
        $<$<CONFIG:MinSizeRel>:${DATYREDB_INTERNAL_COMPILE_OPTIONS_RELEASE}>
    )
    
    # Link options
    target_link_options(${TARGET_NAME} ${VISIBILITY}
        ${DATYREDB_INTERNAL_LINK_OPTIONS}
        $<$<CONFIG:Release>:${DATYREDB_INTERNAL_LINK_OPTIONS_RELEASE}>
        $<$<CONFIG:RelWithDebInfo>:${DATYREDB_INTERNAL_LINK_OPTIONS_RELEASE}>
        $<$<CONFIG:MinSizeRel>:${DATYREDB_INTERNAL_LINK_OPTIONS_RELEASE}>
    )
    
    # C++ standard
    target_compile_features(${TARGET_NAME} ${VISIBILITY} cxx_std_20)
endfunction()

# ==============================================================================
# Private: Apply standard include directories
# ==============================================================================

function(_datyredb_apply_includes TARGET_NAME VISIBILITY)
    target_include_directories(${TARGET_NAME} ${VISIBILITY}
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endfunction()

# ==============================================================================
# Public: Create library target
# ==============================================================================

#[=======================================================================[.rst:
datyredb_add_library
--------------------

Create a library target with standard DatyreDB configuration.

.. code-block:: cmake

  datyredb_add_library(<name>
    [STATIC | SHARED | INTERFACE | OBJECT]
    [ALIAS <alias>]
    [OUTPUT_NAME <name>]
    SOURCES <source1> [<source2> ...]
    [DEPS_PUBLIC <dep1> ...]
    [DEPS_PRIVATE <dep1> ...]
    [DEFINES_PUBLIC <def1> ...]
    [DEFINES_PRIVATE <def1> ...]
  )

#]=======================================================================]
function(datyredb_add_library TARGET_NAME)
    cmake_parse_arguments(PARSE_ARGV 1 ARG
        "STATIC;SHARED;INTERFACE;OBJECT"
        "ALIAS;OUTPUT_NAME"
        "SOURCES;DEPS_PUBLIC;DEPS_PRIVATE;DEFINES_PUBLIC;DEFINES_PRIVATE"
    )
    
    # Validate
    if(NOT ARG_INTERFACE AND NOT ARG_SOURCES)
        message(FATAL_ERROR "datyredb_add_library(${TARGET_NAME}): SOURCES required for non-INTERFACE library")
    endif()
    
    # Determine type
    if(ARG_INTERFACE)
        set(_type INTERFACE)
        set(_public INTERFACE)
        set(_private INTERFACE)
    else()
        if(ARG_OBJECT)
            set(_type OBJECT)
        elseif(ARG_SHARED)
            set(_type SHARED)
        elseif(ARG_STATIC)
            set(_type STATIC)
        elseif(DATYREDB_BUILD_SHARED_LIBS)
            set(_type SHARED)
        else()
            set(_type STATIC)
        endif()
        set(_public PUBLIC)
        set(_private PRIVATE)
    endif()
    
    # Create library
    if(ARG_INTERFACE)
        add_library(${TARGET_NAME} INTERFACE)
    else()
        add_library(${TARGET_NAME} ${_type} ${ARG_SOURCES})
    endif()
    
    # Alias
    if(ARG_ALIAS)
        add_library(${ARG_ALIAS} ALIAS ${TARGET_NAME})
    endif()
    
    # Output name
    if(ARG_OUTPUT_NAME)
        set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${ARG_OUTPUT_NAME}")
    endif()
    
    # Include directories
    _datyredb_apply_includes(${TARGET_NAME} ${_public})
    
    # Compiler options (only for non-INTERFACE, and only at build time)
    if(NOT ARG_INTERFACE)
        _datyredb_apply_compile_options(${TARGET_NAME} PRIVATE)
    endif()
    
    # Dependencies
    if(ARG_DEPS_PUBLIC)
        target_link_libraries(${TARGET_NAME} ${_public} ${ARG_DEPS_PUBLIC})
    endif()
    if(ARG_DEPS_PRIVATE)
        target_link_libraries(${TARGET_NAME} ${_private} ${ARG_DEPS_PRIVATE})
    endif()
    
    # Definitions
    if(ARG_DEFINES_PUBLIC)
        target_compile_definitions(${TARGET_NAME} ${_public} ${ARG_DEFINES_PUBLIC})
    endif()
    if(ARG_DEFINES_PRIVATE)
        target_compile_definitions(${TARGET_NAME} ${_private} ${ARG_DEFINES_PRIVATE})
    endif()
    
    # IDE folder
    set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Libraries")
endfunction()

# ==============================================================================
# Public: Create executable target
# ==============================================================================

#[=======================================================================[.rst:
datyredb_add_executable
-----------------------

Create an executable target with standard DatyreDB configuration.

.. code-block:: cmake

  datyredb_add_executable(<name>
    SOURCES <source1> [<source2> ...]
    [DEPS <dep1> ...]
    [OUTPUT_NAME <name>]
    [FOLDER <folder>]
  )

#]=======================================================================]
function(datyredb_add_executable TARGET_NAME)
    cmake_parse_arguments(PARSE_ARGV 1 ARG
        ""
        "OUTPUT_NAME;FOLDER"
        "SOURCES;DEPS"
    )
    
    # Validate
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "datyredb_add_executable(${TARGET_NAME}): SOURCES required")
    endif()
    
    # Create executable
    add_executable(${TARGET_NAME} ${ARG_SOURCES})
    
    # Output name
    if(ARG_OUTPUT_NAME)
        set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${ARG_OUTPUT_NAME}")
    endif()
    
    # Include directories
    _datyredb_apply_includes(${TARGET_NAME} PRIVATE)
    
    # Compiler options
    _datyredb_apply_compile_options(${TARGET_NAME} PRIVATE)
    
    # Dependencies
    if(ARG_DEPS)
        target_link_libraries(${TARGET_NAME} PRIVATE ${ARG_DEPS})
    endif()
    
    # IDE folder
    set(_folder "Executables")
    if(ARG_FOLDER)
        set(_folder "${ARG_FOLDER}")
    endif()
    set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "${_folder}")
endfunction()

# ==============================================================================
# Public: Create test target
# ==============================================================================

#[=======================================================================[.rst:
datyredb_add_test
-----------------

Create a test executable with GoogleTest integration.

.. code-block:: cmake

  datyredb_add_test(
    NAME <name>
    SOURCES <source1> [<source2> ...]
    [DEPS <dep1> ...]
    [LABELS <label1> ...]
    [TIMEOUT <seconds>]
  )

#]=======================================================================]
function(datyredb_add_test)
    cmake_parse_arguments(PARSE_ARGV 0 ARG
        ""
        "NAME;TIMEOUT"
        "SOURCES;DEPS;LABELS"
    )
    
    # Validate
    if(NOT ARG_NAME)
        message(FATAL_ERROR "datyredb_add_test: NAME required")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "datyredb_add_test(${ARG_NAME}): SOURCES required")
    endif()
    
    # Create executable using our helper
    datyredb_add_executable(${ARG_NAME}
        SOURCES ${ARG_SOURCES}
        DEPS
            DatyreDB::core
            GTest::gtest
            GTest::gtest_main
            fmt::fmt
            ${ARG_DEPS}
        FOLDER "Tests"
    )
    
    # Additional test include path
    target_include_directories(${ARG_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    
    # Output directory
    set_target_properties(${ARG_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
    )
    
    # Timeout
    set(_timeout 60)
    if(ARG_TIMEOUT)
        set(_timeout ${ARG_TIMEOUT})
    endif()
    
    # Register with CTest via GoogleTest discovery
    include(GoogleTest)
    gtest_discover_tests(${ARG_NAME}
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
        PROPERTIES
            LABELS "${ARG_LABELS}"
            TIMEOUT ${_timeout}
        DISCOVERY_TIMEOUT 30
    )
endfunction()

# ==============================================================================
# Public: Create example target
# ==============================================================================

#[=======================================================================[.rst:
datyredb_add_example
--------------------

Create an example executable.

.. code-block:: cmake

  datyredb_add_example(<name>
    SOURCES <source1> [<source2> ...]
    [DEPS <dep1> ...]
  )

#]=======================================================================]
function(datyredb_add_example TARGET_NAME)
    cmake_parse_arguments(PARSE_ARGV 1 ARG
        ""
        ""
        "SOURCES;DEPS"
    )
    
    # Validate
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "datyredb_add_example(${TARGET_NAME}): SOURCES required")
    endif()
    
    # Create executable using our helper
    datyredb_add_executable(${TARGET_NAME}
        SOURCES ${ARG_SOURCES}
        DEPS
            DatyreDB::core
            fmt::fmt
            ${ARG_DEPS}
        FOLDER "Examples"
    )
    
    # Output directory
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples"
    )
endfunction()

# ==============================================================================
# Public: Create benchmark target
# ==============================================================================

#[=======================================================================[.rst:
datyredb_add_benchmark
----------------------

Create a benchmark executable with Google Benchmark integration.

.. code-block:: cmake

  datyredb_add_benchmark(<name>
    SOURCES <source1> [<source2> ...]
    [DEPS <dep1> ...]
  )

#]=======================================================================]
function(datyredb_add_benchmark TARGET_NAME)
    cmake_parse_arguments(PARSE_ARGV 1 ARG
        ""
        ""
        "SOURCES;DEPS"
    )
    
    # Validate
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "datyredb_add_benchmark(${TARGET_NAME}): SOURCES required")
    endif()
    
    # Create executable using our helper
    datyredb_add_executable(${TARGET_NAME}
        SOURCES ${ARG_SOURCES}
        DEPS
            DatyreDB::core
            benchmark::benchmark
            benchmark::benchmark_main
            fmt::fmt
            ${ARG_DEPS}
        FOLDER "Benchmarks"
    )
    
    # Output directory
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/benchmarks"
    )
endfunction()
