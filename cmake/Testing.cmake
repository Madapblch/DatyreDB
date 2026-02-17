# ==============================================================================
# Testing.cmake - Test configuration
# ==============================================================================

include_guard(GLOBAL)

include(CTest)

# ==============================================================================
# GoogleTest
# ==============================================================================

if(DATYREDB_USE_SYSTEM_DEPS)
    find_package(GTest REQUIRED)
else()
    find_package(GTest QUIET)
    
    if(NOT GTest_FOUND)
        message(STATUS "GoogleTest not found - downloading via FetchContent")
        
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
            GIT_SHALLOW    TRUE
            GIT_PROGRESS   TRUE
        )
        
        # Disable install for gtest
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

include(GoogleTest)

# ==============================================================================
# Helper function to create tests
# ==============================================================================

function(datyredb_add_test)
    set(OPTIONS "")
    set(ONE_VALUE_ARGS NAME)
    set(MULTI_VALUE_ARGS SOURCES LIBRARIES LABELS)
    cmake_parse_arguments(ARG "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})
    
    if(NOT ARG_NAME)
        message(FATAL_ERROR "datyredb_add_test: NAME is required")
    endif()
    
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "datyredb_add_test: SOURCES is required")
    endif()
    
    # Create test executable
    add_executable(${ARG_NAME} ${ARG_SOURCES})
    
    # Link libraries
    target_link_libraries(${ARG_NAME}
        PRIVATE
            datyredb::core
            GTest::gtest
            GTest::gtest_main
            ${ARG_LIBRARIES}
    )
    
    # Include directories
    target_include_directories(${ARG_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests
    )
    
    # Apply compiler options
    datyredb_set_target_options(${ARG_NAME})
    
    # Register with CTest
    gtest_discover_tests(${ARG_NAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "${ARG_LABELS}"
        DISCOVERY_TIMEOUT 60
    )
endfunction()
