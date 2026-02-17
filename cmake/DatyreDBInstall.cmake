# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  DatyreDBInstall.cmake - Installation configuration                          ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

include_guard(GLOBAL)

# ==============================================================================
# Install Targets
# ==============================================================================

install(TARGETS datyredb_core
    EXPORT DatyreDBTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT Runtime
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT Development
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT Runtime
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(TARGETS datyredb_server
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT Runtime
)

# ==============================================================================
# Install Headers
# ==============================================================================

install(DIRECTORY "${PROJECT_SOURCE_DIR}/src/datyredb/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/datyredb"
    COMPONENT Development
    FILES_MATCHING 
        PATTERN "*.hpp"
        PATTERN "*.h"
        PATTERN "*.in" EXCLUDE
)

install(DIRECTORY "${PROJECT_BINARY_DIR}/generated/datyredb/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/datyredb"
    COMPONENT Development
    FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
)

# ==============================================================================
# Export Targets
# ==============================================================================

install(EXPORT DatyreDBTargets
    FILE DatyreDBTargets.cmake
    NAMESPACE DatyreDB::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/DatyreDB"
    COMPONENT Development
)

# ==============================================================================
# Package Config
# ==============================================================================

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/DatyreDBConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/DatyreDBConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/DatyreDB"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/DatyreDBConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${PROJECT_BINARY_DIR}/DatyreDBConfig.cmake"
    "${PROJECT_BINARY_DIR}/DatyreDBConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/DatyreDB"
    COMPONENT Development
)

# ==============================================================================
# pkg-config
# ==============================================================================

configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/datyredb.pc.in"
    "${PROJECT_BINARY_DIR}/datyredb.pc"
    @ONLY
)

install(FILES "${PROJECT_BINARY_DIR}/datyredb.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT Development
)
