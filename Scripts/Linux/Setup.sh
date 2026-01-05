#!/bin/bash

echo "=========================================="
echo "    KeyStone Engine - Linux Setup"
echo "=========================================="

echo "[1/3] Checking Premake5..."
if ! command -v premake5 &> /dev/null; then
    echo "[ERROR] premake5 could not be found!"
    echo "Please install it or add it to your PATH."
    exit 1
fi
echo "  - Premake5 found."

echo "[2/3] Checking C++ Compiler..."
COMPILER=""
if command -v g++ &> /dev/null; then
    COMPILER="g++"
elif command -v clang++ &> /dev/null; then
    COMPILER="clang++"
else
    echo "[ERROR] No suitable C++ compiler (g++ or clang++) found!"
    exit 1
fi

VERSION=$($COMPILER -dumpversion | cut -d. -f1)
if [ "$VERSION" -lt 10 ]; then
    echo "[WARNING] Your compiler version ($VERSION) might be too old for C++20."
else
    echo "  - Found $COMPILER (v$VERSION) - Compatible."
fi

echo "[3/3] Setting up Vcpkg & Dependencies..."

if command -v apt-get &> /dev/null; then
    echo "  - Installing system dependencies (libreadline-dev)..."
    sudo apt-get update && sudo apt-get install -y build-essential libreadline-dev
fi

pushd ../../vendor/vcpkg > /dev/null
chmod +x bootstrap-vcpkg.sh
./bootstrap-vcpkg.sh

./vcpkg install lua
./vcpkg install libffi
./vcpkg install spdlog
./vcpkg install rapidjson
./vcpkg install doctest
popd > /dev/null

echo ""
echo "[SUCCESS] Setup completed!"