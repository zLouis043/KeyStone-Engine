@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Keystone Engine - Dependency Setup
echo ========================================

set /p VCPKG_DIR="Enter path where you want to install vcpkg [default: %CD%\vcpkg]: "
if "!VCPKG_DIR!"=="" set VCPKG_DIR=%CD%\vcpkg

echo.
echo Installing vcpkg to: !VCPKG_DIR!
echo.

if not exist "!VCPKG_DIR!\vcpkg.exe" (
    echo Cloning vcpkg repository...
    if exist "!VCPKG_DIR!" (
        echo ERROR: Directory "!VCPKG_DIR!" already exists but is not a vcpkg installation.
        echo Please choose a different path or delete the existing directory.
        pause
        exit /b 1
    )
    
    git clone https://github.com/Microsoft/vcpkg.git "!VCPKG_DIR!"
    if errorlevel 1 (
        echo ERROR: Failed to clone vcpkg repository.
        pause
        exit /b 1
    )
    
    echo Bootstrapping vcpkg...
    cd "!VCPKG_DIR!"
    .\bootstrap-vcpkg.bat
    if errorlevel 1 (
        echo ERROR: Failed to bootstrap vcpkg.
        pause
        exit /b 1
    )
    cd ..
) else (
    echo vcpkg already installed at: !VCPKG_DIR!
)

echo.
echo Installing dependencies...
"!VCPKG_DIR!\vcpkg.exe" install ^
    cxxopts ^
    nlohmann-json ^
    sol2 ^
    lua ^
    raylib

if errorlevel 1 (
    echo ERROR: Failed to install dependencies.
    pause
    exit /b 1
)

echo.
echo Setting environment variable...
setx VCPKG_ROOT "!VCPKG_DIR!"
set VCPKG_ROOT=!VCPKG_DIR!

echo.
echo ========================================
echo Setup completed!
echo VCPKG_ROOT environment variable set to: !VCPKG_DIR!
echo ========================================

echo.
echo Available Premake5 actions:
echo   premake5 vs2022     - Visual Studio 2022
echo   premake5 vs2019     - Visual Studio 2019
echo   premake5 gmake2     - GNU Makefiles (Linux)
echo   premake5 xcode4     - Xcode (macOS)
echo.

choice /c YN /m "Do you want to open a new command prompt with the new environment"
if errorlevel 2 (
    echo.
    echo NOTE: You need to restart your command prompt before using premake5.
    echo Or run manually: set VCPKG_ROOT=!VCPKG_DIR!
    echo.
    pause
) else (
    echo Opening new command prompt...
    start cmd /k "cd /d "%CD%" && echo Keystone environment ready! && echo VCPKG_ROOT=%VCPKG_ROOT% && echo Run: premake5 [action] to generate projects"
)

exit /b 0