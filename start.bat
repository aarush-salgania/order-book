@echo off
setlocal enabledelayedexpansion
set "ROOT_DIR=%~dp0"

echo ========================================================
echo  1. Building Order Book C++ Engine (engine_a_main.cpp)
echo ========================================================

if not exist "%ROOT_DIR%build" mkdir "%ROOT_DIR%build"
cmake -S "%ROOT_DIR%." -B "%ROOT_DIR%build" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [WARN] CMake configure failed. Attempting direct compilation with c++...
    c++ -std=c++20 -O3 -I"%ROOT_DIR%." "%ROOT_DIR%Core\engine_a_main.cpp" -o "%ROOT_DIR%backend\order_book.exe"
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Compilation failed. Please check your C++ compiler.
        pause
        exit /b %ERRORLEVEL%
    )
) else (
    cmake --build "%ROOT_DIR%build" --target order_book_engine --config Release
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Engine build failed.
        pause
        exit /b %ERRORLEVEL%
    )
)

echo [OK] Engine binary ready at backend\order_book.exe!
echo.
echo ========================================================
echo  2. Starting Backend and Frontend Services
echo ========================================================

start "Order Book Backend" cmd /k "cd /d %ROOT_DIR%backend && node index.js"
start "Order Book Frontend" cmd /k "cd /d %ROOT_DIR%frontend && npm run dev"

echo Both services launched in separate windows.

