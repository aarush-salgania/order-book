@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "BASE_DIR=%SCRIPT_DIR%.."
set "BUILD_DIR=%BASE_DIR%\build_tests"

echo === Configuring and Building Test Suite ===
cmake -S "%BASE_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Debug
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake --build "%BUILD_DIR%" --target ob_tests --config Debug
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

set "TEST_EXE=%BUILD_DIR%\ob_tests.exe"
if not exist "%TEST_EXE%" set "TEST_EXE=%BUILD_DIR%\Debug\ob_tests.exe"

echo.
echo === Running Tests ===
"%TEST_EXE%" %*
exit /b %ERRORLEVEL%

