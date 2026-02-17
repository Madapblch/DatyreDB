# ==============================================================================
# DatyreDBCompilerOptions.cmake - Compiler configuration
# ==============================================================================

include_guard(GLOBAL)

# ==============================================================================
# Interface library for compiler options
# ==============================================================================

add_library(datyredb_compiler_options INTERFACE)
add_library(DatyreDB::compiler_options ALIAS datyredb_compiler_options)

# ==============================================================================
# C++ Standard
# ==============================================================================

target_compile_features(datyredb_compiler_options INTERFACE cxx_std_20)

# ==============================================================================
# Warnings
# ==============================================================================

if(DATYREDB_ENABLE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(datyredb_compiler_options INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            -Wcast-align
            -Wconversion
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
            -Wshadow
            -Wsign-conversion
            -Wunused
            -Wnull-dereference
        )
        
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(datyredb_compiler_options INTERFACE
                -Wduplicated-branches
                -Wduplicated-cond
                -Wlogical-op
            )
        endif()
        
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(datyredb_compiler_options INTERFACE
            /W4
            /permissive-
            /Zc:__cplusplus
            /utf-8
        )
    endif()
endif()

# ==============================================================================
# Warnings as errors
# ==============================================================================

if(DATYREDB_ENABLE_WERROR)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(datyredb_compiler_options INTERFACE -Werror)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(datyredb_compiler_options INTERFACE /WX)
    endif()
endif()

# ==============================================================================
# Sanitizers
# ==============================================================================

if(DATYREDB_ENABLE_ASAN)
    target_compile_options(datyredb_compiler_options INTERFACE
        -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(datyredb_compiler_options INTERFACE
        -fsanitize=address)
endif()

if(DATYREDB_ENABLE_TSAN)
    target_compile_options(datyredb_compiler_options INTERFACE
        -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(datyredb_compiler_options INTERFACE
        -fsanitize=thread)
endif()

if(DATYREDB_ENABLE_UBSAN)
    target_compile_options(datyredb_compiler_options INTERFACE
        -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(datyredb_compiler_options INTERFACE
        -fsanitize=undefined)
endif()

if(DATYREDB_ENABLE_MSAN)
    target_compile_options(datyredb_compiler_options INTERFACE
        -fsanitize=memory -fno-omit-frame-pointer)
    target_link_options(datyredb_compiler_options INTERFACE
        -fsanitize=memory)
endif()

# ==============================================================================
# Coverage
# ==============================================================================

if(DATYREDB_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(datyredb_compiler_options INTERFACE
            --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(datyredb_compiler_options INTERFACE
            --coverage)
    endif()
endif()

# ==============================================================================
# LTO
# ==============================================================================

if(DATYREDB_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _lto_supported OUTPUT _lto_error)
    if(_lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "LTO not supported: ${_lto_error}")
    endif()
endif()

# ==============================================================================
# Native optimizations
# ==============================================================================

if(DATYREDB_ENABLE_NATIVE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-march=native" _march_native_supported)
        if(_march_native_supported)
            target_compile_options(datyredb_compiler_options INTERFACE
                $<$<CONFIG:Release>:-march=native>
                $<$<CONFIG:RelWithDebInfo>:-march=native>
            )
        endif()
    endif()
endif()

# ==============================================================================
# Debug/Release flags
# ==============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(datyredb_compiler_options INTERFACE
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
        $<$<CONFIG:MinSizeRel>:-Os -DNDEBUG>
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(datyredb_compiler_options INTERFACE
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
    )
endif()
