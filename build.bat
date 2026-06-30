@echo off
REM ============================================================================
REM  Build the Silent Storm engine -- Release (Win32 / x86).
REM  Needs CMake on PATH and Visual Studio 2022+.
REM ============================================================================
setlocal
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake was not found on PATH.
    echo         Install CMake, or run this from a "Developer Command Prompt for VS 2022".
    pause
    exit /b 1
)
cmake -S "%~dp0." -B "%~dp0build" -A Win32 || (pause & exit /b 1)
cmake --build "%~dp0build" --config Release || (pause & exit /b 1)
echo.
echo [OK] Build finished -- see  build\Release\Game.exe
pause
