@echo off
REM DatyreDB Build Script for Windows
REM Professional build automation

setlocal EnableDelayedExpansion

REM ===== Цвета для вывода (Windows 10+) =====
set "BLUE=[94m"
set "GREEN=[92m"
set "RED=[91m"
set "YELLOW=[93m"
set "NC=[0m"

REM ===== Переменные =====
set "BUILD_TYPE=Release"
set "CLEAN_BUILD=0"
set "RUN_TESTS=0"
set "GENERATOR=MinGW Makefiles"

REM ===== Функции (эмуляция через labels) =====
goto :parse_args

:info
    echo %BLUE%[INFO]%NC% %~1
    goto :eof

:success
    echo %GREEN%[SUCCESS]%NC% %~1
    goto :eof

:error
    echo %RED%[ERROR]%NC% %~1
    exit /b 1

:warning
    echo %YELLOW%[WARNING]%NC% %~1
    goto :eof

REM ===== Проверка зависимостей =====
:check_dependencies
    call :info "Checking build dependencies..."
    
    REM Проверка CMake
    where cmake >nul 2>nul
    if %errorlevel% neq 0 (
        call :error "CMake not found! Download from: https://cmake.org/download/"
        exit /b 1
    )
    
    for /f "tokens=3" %%i in ('cmake --version ^| findstr /R "cmake"') do (
        call :info "Found CMake version: %%i"
        goto :cmake_ok
    )
    :cmake_ok
    
    REM Проверка компилятора
    where g++ >nul 2>nul
    if %errorlevel% equ 0 (
        call :info "Found compiler: MinGW GCC"
        set "GENERATOR=MinGW Makefiles"
        set "BUILD_CMD=mingw32-make"
        goto :compiler_ok
    )
    
    where cl >nul 2>nul
    if %errorlevel% equ 0 (
        call :info "Found compiler: Microsoft Visual C++"
        set "GENERATOR=Visual Studio 17 2022"
        set "BUILD_CMD=msbuild"
        goto :compiler_ok
    )
    
    call :error "No C++ compiler found! Install MinGW or Visual Studio."
    exit /b 1
    
    :compiler_ok
    call :success "All dependencies satisfied!"
    goto :eof

REM ===== Очистка =====
:clean_build
    if exist "build" (
        call :info "Cleaning previous build directory..."
        rmdir /s /q build
        call :success "Clean complete!"
    )
    goto :eof

REM ===== Сборка проекта =====
:build_project
    call :info "Starting build process..."
    call :info "Build type: %BUILD_TYPE%"
    call :info "Generator: %GENERATOR%"
    
    REM Конфигурация CMake
    call :info "Configuring CMake..."
    cmake -S . -B build -G "%GENERATOR%" ^
        -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
        -DCMAKE_CXX_STANDARD=20
    
    if %errorlevel% neq 0 (
        call :error "CMake configuration failed!"
        exit /b 1
    )
    
    call :success "CMake configuration complete!"
    
    REM Сборка
    call :info "Building DatyreDB..."
    cmake --build build --config %BUILD_TYPE% -j 8
    
    if %errorlevel% neq 0 (
        call :error "Build failed!"
        exit /b 1
    )
    
    call :success "Build complete!"
    goto :eof

REM ===== Проверка результата =====
:verify_build
    if exist "build\DatyreDB.exe" (
        call :success "Executable found: build\DatyreDB.exe"
        goto :eof
    )
    
    if exist "build\Release\DatyreDB.exe" (
        call :success "Executable found: build\Release\DatyreDB.exe"
        goto :eof
    )
    
    if exist "build\Debug\DatyreDB.exe" (
        call :success "Executable found: build\Debug\DatyreDB.exe"
        goto :eof
    )
    
    call :error "Build succeeded but executable not found!"
    exit /b 1

REM ===== Запуск тестов =====
:run_tests
    if exist "build\datyredb_tests.exe" (
        call :info "Running tests..."
        cd build
        ctest --output-on-failure --timeout 60
        cd ..
        call :success "All tests passed!"
    ) else (
        call :warning "No test executable found. Skipping tests."
    )
    goto :eof

REM ===== Помощь =====
:show_help
    echo DatyreDB Build Script for Windows
    echo.
    echo Usage: build.bat [OPTIONS]
    echo.
    echo OPTIONS:
    echo     --clean         Clean build directory before building
    echo     --debug         Build in Debug mode (default: Release)
    echo     --release       Build in Release mode
    echo     --tests         Run tests after build
    echo     --help          Show this help message
    echo.
    echo EXAMPLES:
    echo     build.bat                   # Build in Release mode
    echo     build.bat --clean --debug   # Clean build in Debug mode
    echo     build.bat --release --tests # Build and run tests
    echo.
    exit /b 0

REM ===== Парсинг аргументов =====
:parse_args
    if "%~1"=="" goto :start_build
    
    if /i "%~1"=="--clean" (
        set "CLEAN_BUILD=1"
        shift
        goto :parse_args
    )
    
    if /i "%~1"=="--debug" (
        set "BUILD_TYPE=Debug"
        shift
        goto :parse_args
    )
    
    if /i "%~1"=="--release" (
        set "BUILD_TYPE=Release"
        shift
        goto :parse_args
    )
    
    if /i "%~1"=="--tests" (
        set "RUN_TESTS=1"
        shift
        goto :parse_args
    )
    
    if /i "%~1"=="--help" goto :show_help
    if /i "%~1"=="-h" goto :show_help
    
    call :error "Unknown option: %~1. Use --help for usage."
    exit /b 1

REM ===== Главная логика =====
:start_build
    echo.
    call :info "========================================="
    call :info "   DatyreDB Professional Build System   "
    call :info "========================================="
    echo.
    
    call :check_dependencies
    
    if %CLEAN_BUILD% equ 1 (
        call :clean_build
    )
    
    call :build_project
    call :verify_build
    
    if %RUN_TESTS% equ 1 (
        call :run_tests
    )
    
    echo.
    call :success "========================================="
    call :success "   Build Successful!                    "
    call :success "========================================="
    echo.
    call :info "Run the database with:"
    echo     build\DatyreDB.exe
    echo     or
    echo     build\Release\DatyreDB.exe
    echo.
    
    exit /b 0
