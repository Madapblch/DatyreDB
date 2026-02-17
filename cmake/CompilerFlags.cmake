# ==============================================================================
# CompilerFlags.cmake - Compiler flags configuration
# ==============================================================================

include_guard(GLOBAL)

# ==============================================================================
# Warning Flags
# ==============================================================================

set(DATYREDB_WARNING_FLAGS "")
set(DATYREDB_WARNING_FLAGS_WERROR "")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND DATYREDB_WARNING_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wcast-align
        -Wcast-qual
        -Wconversion
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wmissing-declarations
        -Wmissing-include-dirs
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wpacked
        -Wredundant-decls
        -Wshadow
        -Wsign-conversion
        -Wswitch-enum
        -Wundef
        -Wunused
        -Wwrite-strings
        -Wnull-dereference
        -Wstrict-aliasing=2
    )
    
    list(APPEND DATYREDB_WARNING_FLAGS_WERROR
        -Werror=return-type
        -Werror=uninitialized
        -Werror=unused-result
    )
    
    # GCC-specific warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND DATYREDB_WARNING_FLAGS
            -Wduplicated-branches
            -Wduplicated-cond
            -Wlogical-op
            -Wuseless-cast
            -Wno-maybe-uninitialized  # Too many false positives
        )
        
        # GCC 10+
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
            list(APPEND DATYREDB_WARNING_FLAGS
                -Warith-conversion
            )
        endif()
    endif()
    
    # Clang-specific warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        list(APPEND DATYREDB_WARNING_FLAGS
            -Wdocumentation
            -Wheader-hygiene
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
        )
    endif()
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND DATYREDB_WARNING_FLAGS
        /W4
        /permissive-
        /Zc:__cplusplus
        /Zc:preprocessor
        /Zc:inline
        /utf-8
        /wd4100  # unreferenced formal parameter
        /wd4127  # conditional expression is constant
        /wd4324  # structure was padded
    )
    
    list(APPEND DATYREDB_WARNING_FLAGS_WERROR
        /WX
    )
endif()

# ==============================================================================
# Optimization Flags
# ==============================================================================

set(DATYREDB_DEBUG_FLAGS "")
set(DATYREDB_RELEASE_FLAGS "")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Debug
    list(APPEND DATYREDB_DEBUG_FLAGS
        -O0
        -g3
        -ggdb
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
    )
    
    # Release
    list(APPEND DATYREDB_RELEASE_FLAGS
        -O3
        -DNDEBUG
        -ffunction-sections
        -fdata-sections
    )
    
    # Native CPU optimizations
    if(DATYREDB_ENABLE_NATIVE)
        check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
        if(COMPILER_SUPPORTS_MARCH_NATIVE)
            list(APPEND DATYREDB_RELEASE_FLAGS -march=native)
            message(STATUS "Native CPU optimizations enabled")
        endif()
    endif()
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Debug
    list(APPEND DATYREDB_DEBUG_FLAGS
        /Od
        /Zi
        /RTC1
        /JMC  # Just My Code debugging
    )
    
    # Release
    list(APPEND DATYREDB_RELEASE_FLAGS
        /O2
        /Oi
        /Ot
        /GL
        /Gy
        /DNDEBUG
    )
endif()

# ==============================================================================
# Link Time Optimization
# ==============================================================================

if(DATYREDB_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)
    
    if(LTO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        message(STATUS "Link Time Optimization enabled")
    else()
        message(WARNING "LTO not supported: ${LTO_ERROR}")
    endif()
endif()

# ==============================================================================
# Linker Flags
# ==============================================================================

set(DATYREDB_LINK_FLAGS "")
set(DATYREDB_LINK_FLAGS_RELEASE "")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Remove unused sections
    if(NOT APPLE)
        list(APPEND DATYREDB_LINK_FLAGS_RELEASE
            -Wl,--gc-sections
            -Wl,--as-needed
        )
    endif()
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND DATYREDB_LINK_FLAGS_RELEASE
        /LTCG
        /OPT:REF
        /OPT:ICF
    )
endif()

# ==============================================================================
# Function to apply flags to a target
# ==============================================================================

function(datyredb_set_target_options TARGET)
    # Parse arguments
    set(OPTIONS PUBLIC PRIVATE)
    set(ONE_VALUE_ARGS "")
    set(MULTI_VALUE_ARGS "")
    cmake_parse_arguments(ARG "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})
    
    if(ARG_PUBLIC)
        set(VISIBILITY PUBLIC)
    else()
        set(VISIBILITY PRIVATE)
    endif()
    
    # Warning flags
    target_compile_options(${TARGET} ${VISIBILITY}
        ${DATYREDB_WARNING_FLAGS}
        ${DATYREDB_WARNING_FLAGS_WERROR}
    )
    
    # Debug/Release flags
    target_compile_options(${TARGET} ${VISIBILITY}
        $<$<CONFIG:Debug>:${DATYREDB_DEBUG_FLAGS}>
        $<$<CONFIG:Release>:${DATYREDB_RELEASE_FLAGS}>
        $<$<CONFIG:RelWithDebInfo>:${DATYREDB_RELEASE_FLAGS} -g>
        $<$<CONFIG:MinSizeRel>:-Os -DNDEBUG>
    )
    
    # Link flags
    target_link_options(${TARGET} ${VISIBILITY}
        ${DATYREDB_LINK_FLAGS}
        $<$<CONFIG:Release>:${DATYREDB_LINK_FLAGS_RELEASE}>
    )
    
    # Sanitizer flags
    if(DATYREDB_SANITIZER_FLAGS)
        target_compile_options(${TARGET} ${VISIBILITY} ${DATYREDB_SANITIZER_FLAGS})
        target_link_options(${TARGET} ${VISIBILITY} ${DATYREDB_SANITIZER_FLAGS})
    endif()
    
    # Coverage flags
    if(DATYREDB_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${TARGET} ${VISIBILITY} --coverage -fprofile-arcs -ftest-coverage)
            target_link_options(${TARGET} ${VISIBILITY} --coverage)
        endif()
    endif()
endfunction()
