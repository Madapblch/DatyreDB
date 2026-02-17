# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  DatyreDBSanitizers.cmake - Sanitizer configuration                          ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

include_guard(GLOBAL)

set(DATYREDB_SANITIZER_FLAGS "")
set(DATYREDB_SANITIZER_LINK_FLAGS "")

# ==============================================================================
# AddressSanitizer
# ==============================================================================

if(DATYREDB_ENABLE_ASAN)
    if(DATYREDB_COMPILER_IS_GNU_LIKE)
        list(APPEND DATYREDB_SANITIZER_FLAGS
            -fsanitize=address
            -fsanitize-address-use-after-scope
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            -fno-common
        )
        list(APPEND DATYREDB_SANITIZER_LINK_FLAGS -fsanitize=address)
        
        # Clang extras
        if(DATYREDB_COMPILER_IS_CLANG)
            list(APPEND DATYREDB_SANITIZER_FLAGS
                -fsanitize-address-use-odr-indicator
            )
        endif()
        
        message(STATUS "AddressSanitizer enabled")
        
    elseif(DATYREDB_COMPILER_IS_MSVC)
        list(APPEND DATYREDB_SANITIZER_FLAGS /fsanitize=address)
        message(STATUS "AddressSanitizer enabled (MSVC)")
    endif()
endif()

# ==============================================================================
# ThreadSanitizer
# ==============================================================================

if(DATYREDB_ENABLE_TSAN)
    if(DATYREDB_COMPILER_IS_GNU_LIKE)
        list(APPEND DATYREDB_SANITIZER_FLAGS
            -fsanitize=thread
            -fno-omit-frame-pointer
            -fPIE
        )
        list(APPEND DATYREDB_SANITIZER_LINK_FLAGS -fsanitize=thread -pie)
        message(STATUS "ThreadSanitizer enabled")
    else()
        message(WARNING "ThreadSanitizer not supported on this compiler")
    endif()
endif()

# ==============================================================================
# UndefinedBehaviorSanitizer
# ==============================================================================

if(DATYREDB_ENABLE_UBSAN)
    if(DATYREDB_COMPILER_IS_GNU_LIKE)
        list(APPEND DATYREDB_SANITIZER_FLAGS
            -fsanitize=undefined
            -fsanitize=float-divide-by-zero
            -fsanitize=integer
            -fsanitize=nullability
            -fno-sanitize-recover=all
            -fno-omit-frame-pointer
        )
        list(APPEND DATYREDB_SANITIZER_LINK_FLAGS -fsanitize=undefined)
        message(STATUS "UndefinedBehaviorSanitizer enabled")
    else()
        message(WARNING "UBSan not supported on this compiler")
    endif()
endif()

# ==============================================================================
# MemorySanitizer (Clang only)
# ==============================================================================

if(DATYREDB_ENABLE_MSAN)
    if(DATYREDB_COMPILER_IS_CLANG AND NOT DATYREDB_COMPILER_IS_APPLECLANG)
        list(APPEND DATYREDB_SANITIZER_FLAGS
            -fsanitize=memory
            -fsanitize-memory-track-origins=2
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            -fPIE
        )
        list(APPEND DATYREDB_SANITIZER_LINK_FLAGS -fsanitize=memory -pie)
        message(STATUS "MemorySanitizer enabled")
    else()
        message(WARNING "MemorySanitizer only supported on Clang (non-Apple)")
        set(DATYREDB_ENABLE_MSAN OFF CACHE BOOL "" FORCE)
    endif()
endif()
