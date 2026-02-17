# ==============================================================================
# Sanitizers.cmake - Sanitizer configuration
# ==============================================================================

include_guard(GLOBAL)

set(DATYREDB_SANITIZER_FLAGS "")

# Check for incompatible options
if(DATYREDB_ENABLE_ASAN AND DATYREDB_ENABLE_TSAN)
    message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer cannot be used together")
endif()

if(DATYREDB_ENABLE_ASAN AND DATYREDB_ENABLE_MSAN)
    message(FATAL_ERROR "AddressSanitizer and MemorySanitizer cannot be used together")
endif()

if(DATYREDB_ENABLE_TSAN AND DATYREDB_ENABLE_MSAN)
    message(FATAL_ERROR "ThreadSanitizer and MemorySanitizer cannot be used together")
endif()

# AddressSanitizer
if(DATYREDB_ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND DATYREDB_SANITIZER_FLAGS
            -fsanitize=address
            -fsanitize-address-use-after-scope
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        list(APPEND DATYREDB_SANITIZER_FLAGS
            /fsanitize=address
        )
    endif()
endif()

# ThreadSanitizer
if(DATYREDB_ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND DATYREDB_SANITIZER_FLAGS
            -fsanitize=thread
            -fno-omit-frame-pointer
        )
    else()
        message(WARNING "ThreadSanitizer not supported on this compiler")
    endif()
endif()

# UndefinedBehaviorSanitizer
if(DATYREDB_ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND DATYREDB_SANITIZER_FLAGS
            -fsanitize=undefined
            -fsanitize=float-divide-by-zero
            -fsanitize=integer
            -fsanitize=nullability
            -fno-sanitize-recover=all
            -fno-omit-frame-pointer
        )
    else()
        message(WARNING "UndefinedBehaviorSanitizer not supported on this compiler")
    endif()
endif()

# MemorySanitizer
if(DATYREDB_ENABLE_MSAN)
    message(STATUS "MemorySanitizer enabled")
    
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        list(APPEND DATYREDB_SANITIZER_FLAGS
            -fsanitize=memory
            -fsanitize-memory-track-origins=2
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
        )
    else()
        message(WARNING "MemorySanitizer only supported on Clang")
    endif()
endif()
