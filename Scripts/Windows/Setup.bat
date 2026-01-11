@echo off
setlocal enabledelayedexpansion

echo ==========================================
echo    KeyStone Engine - Windows Setup
echo ==========================================

echo [1/3] Checking Premake5...
where premake5 >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] premake5 not found in PATH!
    echo Please download it from https://premake.github.io/ and add it to your PATH.
    pause
    exit /b 1
)
echo   - Premake5 found.

echo [2/3] Checking C++ Compiler (MSVC)...
where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo [WARNING] 'cl.exe' not found.
    echo You might be running this outside the 'Developer Command Prompt for VS 2022'.
    echo Ensure you have Visual Studio 2022 installed with C++ Desktop Development workload.
) else (
    echo   - MSVC Compiler found.
)
echo [3/3] Setting up Dependencies via Vcpkg...
pushd ..\..\vendor\vcpkg
if not exist vcpkg.exe (
    call bootstrap-vcpkg.bat
)

vcpkg.exe install lua:x64-windows-static
vcpkg.exe install libffi:x64-windows-static
vcpkg.exe install spdlog:x64-windows-static
vcpkg.exe install rapidjson:x64-windows-static
vcpkg.exe install flecs:x64-windows-static
vcpkg.exe install doctest:x64-windows-static
popd

echo.
echo [SUCCESS] Setup completed!
echo You can now run 'GenerateProjects.bat'.
pause