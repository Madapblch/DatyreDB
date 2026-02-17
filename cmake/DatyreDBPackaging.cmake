# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  DatyreDBPackaging.cmake - CPack configuration                               ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

include_guard(GLOBAL)

# ==============================================================================
# General CPack Settings
# ==============================================================================

set(CPACK_PACKAGE_NAME "datyredb")
set(CPACK_PACKAGE_VENDOR "DatyreDB Authors")
set(CPACK_PACKAGE_CONTACT "datyredb@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION "DatyreDB is a high-performance embedded database engine written in C++20. It features ACID transactions, MVCC, Write-Ahead Logging, and intelligent checkpointing.")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${DATYREDB_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DATYREDB_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DATYREDB_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "DatyreDB")
set(CPACK_PACKAGE_CHECKSUM "SHA256")

# Resource files
if(EXISTS "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/README.md")
    set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
endif()

# ==============================================================================
# Source Package
# ==============================================================================

set(CPACK_SOURCE_GENERATOR "TGZ;TXZ;ZIP")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-src")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\\\.git/"
    "/\\\\.github/"
    "/\\\\.vscode/"
    "/\\\\.idea/"
    "/build/"
    "/install/"
    "/\\\\.cache/"
    "\\\\.gitignore"
    "\\\\.clang-format"
    "\\\\.clang-tidy"
    "CMakeUserPresets\\\\.json"
)

# ==============================================================================
# Binary Package
# ==============================================================================

set(CPACK_GENERATOR "")

# Detect available generators
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Check for dpkg
    find_program(DPKG_EXECUTABLE dpkg)
    if(DPKG_EXECUTABLE)
        list(APPEND CPACK_GENERATOR "DEB")
    endif()
    
    # Check for rpmbuild
    find_program(RPMBUILD_EXECUTABLE rpmbuild)
    if(RPMBUILD_EXECUTABLE)
        list(APPEND CPACK_GENERATOR "RPM")
    endif()
    
    # Always include TGZ
    list(APPEND CPACK_GENERATOR "TGZ")
    
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND CPACK_GENERATOR "TGZ" "productbuild")
    
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND CPACK_GENERATOR "ZIP" "NSIS")
endif()

# Fallback
if(NOT CPACK_GENERATOR)
    set(CPACK_GENERATOR "TGZ")
endif()

# ==============================================================================
# DEB Package (Debian/Ubuntu)
# ==============================================================================

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "database")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17), libstdc++6 (>= 10)")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)

# Component-based packaging
set(CPACK_DEBIAN_PACKAGE_COMPONENT ON)
set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "datyredb")
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_NAME "datyredb-dev")
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_DEPENDS "datyredb (= ${PROJECT_VERSION})")

# ==============================================================================
# RPM Package (RHEL/CentOS/Fedora)
# ==============================================================================

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Databases")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc >= 2.17, libstdc++ >= 10")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_AUTOREQ OFF)
set(CPACK_RPM_PACKAGE_AUTOPROV OFF)

# Component-based packaging
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_RUNTIME_PACKAGE_NAME "datyredb")
set(CPACK_RPM_DEVELOPMENT_PACKAGE_NAME "datyredb-devel")

# ==============================================================================
# Components
# ==============================================================================

set(CPACK_COMPONENTS_ALL Runtime Development)

set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "DatyreDB runtime files and executables")
set(CPACK_COMPONENT_RUNTIME_REQUIRED ON)

set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Development")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION "Development files (headers, libraries, CMake config)")
set(CPACK_COMPONENT_DEVELOPMENT_DEPENDS Runtime)

# ==============================================================================
# Archive naming
# ==============================================================================

set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# ==============================================================================
# Include CPack
# ==============================================================================

include(CPack)

# ==============================================================================
# Custom packaging targets
# ==============================================================================

add_custom_target(package-source
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target package_source
    COMMENT "Creating source package"
    VERBATIM
)

add_custom_target(package-binary
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target package
    COMMENT "Creating binary package"
    VERBATIM
)
