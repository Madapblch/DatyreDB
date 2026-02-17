# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  benchmarks/CMakeLists.txt - Performance benchmarks                          ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

datyredb_add_benchmark(bench_page
    SOURCES bench_page.cpp
)

datyredb_add_benchmark(bench_buffer_pool
    SOURCES bench_buffer_pool.cpp
)

datyredb_add_benchmark(bench_storage_engine
    SOURCES bench_storage_engine.cpp
)

# ==============================================================================
# Run Benchmarks Target
# ==============================================================================

add_custom_target(run-benchmarks
    COMMAND bench_page --benchmark_format=console
    COMMAND bench_buffer_pool --benchmark_format=console
    COMMAND bench_storage_engine --benchmark_format=console
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all benchmarks"
    USES_TERMINAL
)
