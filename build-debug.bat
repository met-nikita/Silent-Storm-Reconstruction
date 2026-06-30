@echo off
REM ============================================================================
REM  Build the Silent Storm engine -- RelWithDebInfo (optimized + PDBs).
REM  Afterwards open  build\A5.sln  in Visual Studio (Game is the startup
REM  project) and press F5 -- it runs from your game install dir.
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
cmake --build "%~dp0build" --config RelWithDebInfo || (pause & exit /b 1)
echo.
echo [OK] Build finished. Open  build\A5.sln  in Visual Studio and press F5 to debug.
pause
