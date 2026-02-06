#!/bin/bash
set -e # Остановить скрипт при любой ошибке

echo "--- [1/3] Building DatyreDB in Release mode ---"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target datyredb_server datyredb_tests_suite

echo "--- [2/3] Running C++ Unit Tests ---"
cd build && ctest --output-on-failure
cd ..

echo "--- [3/3] Running Python Integration Tests ---"
python3 tests/integration/test_system.py

echo "--- ALL AUDITS PASSED ---"