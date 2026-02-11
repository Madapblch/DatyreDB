#!/usr/bin/env bash
# DatyreDB Build Script
# Professional build automation for Linux/macOS/WSL

set -e  # Остановить выполнение при любой ошибке
set -u  # Ошибка при использовании неопределенных переменных

# ===== Цвета для вывода =====
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ===== Функции для красивого вывода =====
info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# ===== Проверка зависимостей =====
check_dependencies() {
    info "Checking build dependencies..."
    
    # Проверка CMake
    if ! command -v cmake &> /dev/null; then
        error "CMake is not installed. Install it with: sudo apt install cmake"
    fi
    
    CMAKE_VERSION=$(cmake --version | head -n1 | grep -oP '\d+\.\d+\.\d+')
    info "Found CMake version: $CMAKE_VERSION"
    
    # Проверка компилятора C++
    if command -v g++ &> /dev/null; then
        CXX_COMPILER="g++"
        CXX_VERSION=$(g++ --version | head -n1)
        info "Found compiler: $CXX_VERSION"
    elif command -v clang++ &> /dev/null; then
        CXX_COMPILER="clang++"
        CXX_VERSION=$(clang++ --version | head -n1)
        info "Found compiler: $CXX_VERSION"
    else
        error "No C++ compiler found. Install with: sudo apt install build-essential"
    fi
    
    # Проверка Git
    if ! command -v git &> /dev/null; then
        warning "Git is not installed. You won't be able to clone repositories."
    fi
    
    success "All dependencies are satisfied!"
}

# ===== Определение количества потоков для сборки =====
get_num_threads() {
    if command -v nproc &> /dev/null; then
        nproc
    elif command -v sysctl &> /dev/null; then
        sysctl -n hw.ncpu 2>/dev/null || echo 2
    else
        echo 2
    fi
}

# ===== Очистка предыдущей сборки =====
clean_build() {
    if [ -d "build" ]; then
        info "Cleaning previous build directory..."
        rm -rf build
        success "Clean complete!"
    fi
}

# ===== Основная функция сборки =====
build_project() {
    local BUILD_TYPE="${1:-Release}"
    local NUM_THREADS=$(get_num_threads)
    
    info "Starting build process..."
    info "Build type: $BUILD_TYPE"
    info "Using $NUM_THREADS threads"
    
    # Конфигурация CMake
    info "Configuring CMake..."
    cmake -S . -B build \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        || error "CMake configuration failed!"
    
    success "CMake configuration complete!"
    
    # Сборка проекта
    info "Building DatyreDB..."
    cmake --build build --config "$BUILD_TYPE" -j "$NUM_THREADS" \
        || error "Build failed!"
    
    success "Build complete!"
}

# ===== Проверка результата сборки =====
verify_build() {
    if [ -f "build/DatyreDB" ]; then
        success "Executable found: build/DatyreDB"
        
        # Показать информацию о бинарнике
        local SIZE=$(du -h build/DatyreDB | cut -f1)
        info "Binary size: $SIZE"
        
        # Проверить зависимости (Linux)
        if command -v ldd &> /dev/null; then
            info "Dynamic dependencies:"
            ldd build/DatyreDB | grep -E "libc|libstdc++" || true
        fi
        
        return 0
    else
        error "Build succeeded but executable not found!"
    fi
}

# ===== Запуск тестов =====
run_tests() {
    if [ -f "build/datyredb_tests" ]; then
        info "Running tests..."
        cd build
        ctest --output-on-failure --timeout 60
        cd ..
        success "All tests passed!"
    else
        warning "No test executable found. Skipping tests."
    fi
}

# ===== Показать помощь =====
show_help() {
    cat << EOF
DatyreDB Build Script

Usage: $0 [OPTIONS]

OPTIONS:
    --clean         Clean build directory before building
    --debug         Build in Debug mode (default: Release)
    --release       Build in Release mode
    --tests         Run tests after build
    --help          Show this help message

EXAMPLES:
    $0                      # Build in Release mode
    $0 --clean --debug      # Clean build in Debug mode
    $0 --release --tests    # Build and run tests

EOF
}

# ===== Главная логика =====
main() {
    echo ""
    info "========================================="
    info "   DatyreDB Professional Build System   "
    info "========================================="
    echo ""
    
    # Параметры по умолчанию
    BUILD_TYPE="Release"
    CLEAN=false
    RUN_TESTS=false
    
    # Парсинг аргументов командной строки
    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean)
                CLEAN=true
                shift
                ;;
            --debug)
                BUILD_TYPE="Debug"
                shift
                ;;
            --release)
                BUILD_TYPE="Release"
                shift
                ;;
            --tests)
                RUN_TESTS=true
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                error "Unknown option: $1. Use --help for usage."
                ;;
        esac
    done
    
    # Выполнение
    check_dependencies
    
    if [ "$CLEAN" = true ]; then
        clean_build
    fi
    
    build_project "$BUILD_TYPE"
    verify_build
    
    if [ "$RUN_TESTS" = true ]; then
        run_tests
    fi
    
    echo ""
    success "========================================="
    success "   Build Successful!                    "
    success "========================================="
    echo ""
    info "Run the database with:"
    echo -e "    ${GREEN}./build/DatyreDB${NC}"
    echo ""
    
    if [ "$BUILD_TYPE" = "Debug" ]; then
        info "Debug build includes:"
        echo "  - Debug symbols (use with gdb)"
        echo "  - Address sanitizer (detects memory leaks)"
        echo "  - Assertions enabled"
    fi
}

# Запуск главной функции
main "$@"
