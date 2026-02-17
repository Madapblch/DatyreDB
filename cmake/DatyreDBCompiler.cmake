# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  DatyreDBCompiler.cmake - Compiler configuration                             ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

include_guard(GLOBAL)

# ==============================================================================
# Compiler Identification
# ==============================================================================

set(DATYREDB_COMPILER_IS_GNU OFF)
set(DATYREDB_COMPILER_IS_CLANG OFF)
set(DATYREDB_COMPILER_IS_APPLECLANG OFF)
set(DATYREDB_COMPILER_IS_MSVC OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(DATYREDB_COMPILER_IS_GNU ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(DATYREDB_COMPILER_IS_CLANG ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(DATYREDB_COMPILER_IS_APPLECLANG ON)
    set(DATYREDB_COMPILER_IS_CLANG ON)  # AppleClang is also Clang
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(DATYREDB_COMPILER_IS_MSVC ON)
endif()

set(DATYREDB_COMPILER_IS_GNU_LIKE OFF)
if(DATYREDB_COMPILER_IS_GNU OR DATYREDB_COMPILER_IS_CLANG)
    set(DATYREDB_COMPILER_IS_GNU_LIKE ON)
endif()

# ==============================================================================
# Warning Flags Storage
# ==============================================================================

set(DATYREDB_CXX_FLAGS "")
set(DATYREDB_CXX_FLAGS_DEBUG "")
set(DATYREDB_CXX_FLAGS_RELEASE "")
set(DATYREDB_LINK_FLAGS "")
set(DATYREDB_LINK_FLAGS_RELEASE "")

# ==============================================================================
# Warning Flags
# ==============================================================================

if(DATYREDB_ENABLE_WARNINGS)
    if(DATYREDB_COMPILER_IS_GNU_LIKE)
        list(APPEND DATYREDB_CXX_FLAGS
            -Wall
            -Wextra
            -Wcast-align
            -Wcast-qual
            -Wconversion
            -Wdouble-promotion
            -Wduplicated-cond
            -Wfloat-equal
            -Wformat=2
            -Wimplicit-fallthrough
            -Wmissing-declarations
            -Wmissing-include-dirs
            -Wnon-virtual-dtor
            -Wnull-dereference
            -Wold-style-cast
            -Woverloaded-virtual
            -Wpacked
            -Wpointer-arith
            -Wredundant-decls
            -Wshadow
            -Wsign-conversion
            -Wstrict-aliasing=2
            -Wswitch-enum
            -Wundef
            -Wunused
            -Wwrite-strings
        )
        
        if(DATYREDB_ENABLE_PEDANTIC)
            list(APPEND DATYREDB_CXX_FLAGS -Wpedantic)
        endif()
        
        # GCC-specific
        if(DATYREDB_COMPILER_IS_GNU)
            list(APPEND DATYREDB_CXX_FLAGS
                -Wduplicated-branches
                -Wlogical-op
                -Wuseless-cast
            )
            
            # GCC 10+
            if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
                list(APPEND DATYREDB_CXX_FLAGS -Warith-conversion)
            endif()
            
            # GCC 12+
            if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
                list(APPEND DATYREDB_CXX_FLAGS -Wtrivial-auto-var-init)
            endif()
        endif()
        
        # Clang-specific
        if(DATYREDB_COMPILER_IS_CLANG AND NOT DATYREDB_COMPILER_IS_APPLECLANG)
            list(APPEND DATYREDB_CXX_FLAGS
                -Wdocumentation
                -Wheader-hygiene
                -Wno-c++98-compat
                -Wno-c++98-compat-pedantic
            )
        endif()
        
    elseif(DATYREDB_COMPILER_IS_MSVC)
        list(APPEND DATYREDB_CXX_FLAGS
            /W4
            /permissive-
            /Zc:__cplusplus
            /Zc:enumTypes
            /Zc:externConstexpr
            /Zc:inline
            /Zc:lambda
            /Zc:preprocessor
            /Zc:referenceBinding
            /Zc:rvalueCast
            /Zc:strictStrings
            /Zc:ternary
            /Zc:throwingNew
            /utf-8
            /volatile:iso
            /wd4100  # unreferenced formal parameter
            /wd4127  # conditional expression is constant
            /wd4324  # structure was padded due to alignment
        )
    endif()
    
    # Werror
    if(DATYREDB_ENABLE_WERROR)
        if(DATYREDB_COMPILER_IS_GNU_LIKE)
            list(APPEND DATYREDB_CXX_FLAGS -Werror)
        elseif(DATYREDB_COMPILER_IS_MSVC)
            list(APPEND DATYREDB_CXX_FLAGS /WX)
        endif()
    endif()
endif()

# ==============================================================================
# Optimization Flags
# ==============================================================================

if(DATYREDB_COMPILER_IS_GNU_LIKE)
    list(APPEND DATYREDB_CXX_FLAGS_DEBUG
        -O0
        -g3
        -ggdb
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
    )
    
    list(APPEND DATYREDB_CXX_FLAGS_RELEASE
        -O3
        -DNDEBUG
        -ffunction-sections
        -fdata-sections
    )
    
    if(NOT APPLE)
        list(APPEND DATYREDB_LINK_FLAGS_RELEASE
            -Wl,--gc-sections
            -Wl,--as-needed
        )
    endif()
    
    # Native optimizations
    if(DATYREDB_ENABLE_NATIVE)
        check_cxx_compiler_flag("-march=native" _march_native_supported)
        if(_march_native_supported)
            list(APPEND DATYREDB_CXX_FLAGS_RELEASE -march=native)
            message(STATUS "Native CPU optimizations enabled")
        endif()
    endif()
    
elseif(DATYREDB_COMPILER_IS_MSVC)
    list(APPEND DATYREDB_CXX_FLAGS_DEBUG
        /Od
        /Zi
        /RTC1
        /JMC
        /sdl
    )
    
    list(APPEND DATYREDB_CXX_FLAGS_RELEASE
        /O2
        /Oi
        /Ot
        /GL
        /Gy
        /DNDEBUG
    )
    
    list(APPEND DATYREDB_LINK_FLAGS_RELEASE
        /LTCG
        /OPT:REF
        /OPT:ICF
    )
endif()

# ==============================================================================
# LTO
# ==============================================================================

if(DATYREDB_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _lto_supported OUTPUT _lto_error)
    
    if(_lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
        message(STATUS "Link Time Optimization enabled")
    else()
        message(WARNING "LTO not supported: ${_lto_error}")
        set(DATYREDB_ENABLE_LTO OFF CACHE BOOL "" FORCE)
    endif()
endif()

# ==============================================================================
# Coverage
# ==============================================================================

if(DATYREDB_ENABLE_COVERAGE)
    if(DATYREDB_COMPILER_IS_GNU_LIKE)
        list(APPEND DATYREDB_CXX_FLAGS --coverage -fprofile-arcs -ftest-coverage)
        list(APPEND DATYREDB_LINK_FLAGS --coverage)
        message(STATUS "Code coverage enabled")
    else()
        message(WARNING "Coverage not supported on this compiler")
        set(DATYREDB_ENABLE_COVERAGE OFF CACHE BOOL "" FORCE)
    endif()
endif()
