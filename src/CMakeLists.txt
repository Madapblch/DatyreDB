# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  src/CMakeLists.txt - Main library and executable                            ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

# ==============================================================================
# Source Files
# ==============================================================================

set(DATYREDB_SOURCES
    # Utils
    internal/utils/logger.cpp
    
    # Storage
    internal/storage/page.cpp
    internal/storage/disk_manager.cpp
    internal/storage/buffer_pool.cpp
    internal/storage/wal.cpp
    internal/storage/checkpoint.cpp
    
    # Core
    internal/core/storage_engine.cpp
    internal/core/database.cpp
    internal/core/query_executor.cpp
    internal/core/table.cpp
)

# ==============================================================================
# Core Library
# ==============================================================================

datyredb_add_library(datyredb_core
    STATIC
    ALIAS DatyreDB::core
    OUTPUT_NAME datyredb
    
    SOURCES ${DATYREDB_SOURCES}
    
    DEPS_PUBLIC
        Threads::Threads
    
    DEPS_PRIVATE
        $<BUILD_INTERFACE:fmt::fmt>
        $<$<BOOL:${DATYREDB_HAS_SPDLOG}>:$<BUILD_INTERFACE:spdlog::spdlog>>
    
    DEFINES_PUBLIC
        $<$<BOOL:${DATYREDB_ENABLE_LOGGING}>:DATYREDB_ENABLE_LOGGING>
        $<$<BOOL:${DATYREDB_ENABLE_ASSERTIONS}>:DATYREDB_ENABLE_ASSERTIONS>
        $<$<BOOL:${DATYREDB_HAS_SPDLOG}>:DATYREDB_HAS_SPDLOG>
    
    DEFINES_PRIVATE
        DATYREDB_VERSION="${PROJECT_VERSION}"
        DATYREDB_VERSION_MAJOR=${DATYREDB_VERSION_MAJOR}
        DATYREDB_VERSION_MINOR=${DATYREDB_VERSION_MINOR}
        DATYREDB_VERSION_PATCH=${DATYREDB_VERSION_PATCH}
)

set_target_properties(datyredb_core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${DATYREDB_VERSION_MAJOR}
    EXPORT_NAME core
)

# ==============================================================================
# Server Executable
# ==============================================================================

datyredb_add_executable(datyredb_server
    OUTPUT_NAME datyredb
    FOLDER "Executables"
    
    SOURCES main.cpp
    
    DEPS
        DatyreDB::core
        fmt::fmt
)
