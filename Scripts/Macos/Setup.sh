#!/bin/bash

echo "=========================================="
echo "    KeyStone Engine - MacOS Setup"
echo "=========================================="

echo "[0/3] Updating Git Submodules..."
git submodule update --init --recursive
if [ $? -ne 0 ]; then
    echo "[WARNING] Git command failed. Ensure Git is installed and in PATH."
fi

echo "[1/3] Checking Premake5..."
if ! command -v premake5 &> /dev/null; then
    echo "[ERROR] premake5 not found!"
    echo "Tip: Run 'brew install premake'"
    exit 1
fi
echo "  - Premake5 found."

echo "[2/3] Checking C++ Compiler..."
if ! command -v clang++ &> /dev/null; then
    echo "[ERROR] clang++ not found! Install Xcode Command Line Tools."
    exit 1
fi
echo "  - Clang found."

echo "[3/3] Setting up Vcpkg & Dependencies..."
pushd ../../vendor/vcpkg > /dev/null
chmod +x bootstrap-vcpkg.sh
./bootstrap-vcpkg.sh

./vcpkg install lua
./vcpkg install libffi
./vcpkg install spdlog
./vcpkg install rapidjson
./vcpkg install flecs
./vcpkg install doctest
./vcpkg install pegtl
popd > /dev/null

echo ""
echo "[SUCCESS] Setup completed!"