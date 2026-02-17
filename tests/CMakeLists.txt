# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  tests/CMakeLists.txt - Test suite                                           ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

# ==============================================================================
# Unit Tests
# ==============================================================================

datyredb_add_test(NAME test_page
    SOURCES unit/test_page.cpp
    LABELS unit storage
)

datyredb_add_test(NAME test_buffer_pool
    SOURCES unit/test_buffer_pool.cpp
    LABELS unit storage
)

datyredb_add_test(NAME test_wal
    SOURCES unit/test_wal.cpp
    LABELS unit storage
)

datyredb_add_test(NAME test_checkpoint
    SOURCES unit/test_checkpoint.cpp
    LABELS unit checkpoint
)

datyredb_add_test(NAME test_storage_engine
    SOURCES unit/test_storage_engine.cpp
    LABELS unit engine
)

# ==============================================================================
# Custom Targets for Convenience
# ==============================================================================

add_custom_target(test-unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure -j
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT "Running unit tests"
    USES_TERMINAL
)

add_custom_target(test-storage
    COMMAND ${CMAKE_CTEST_COMMAND} -L storage --output-on-failure -j
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT "Running storage tests"
    USES_TERMINAL
)

add_custom_target(test-all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -j
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT "Running all tests"
    USES_TERMINAL
)
