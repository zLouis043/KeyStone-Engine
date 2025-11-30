#!/bin/bash
echo "[KeyStone] Cleaning Build Artifacts..."
cd ../../KeyStoneEngine
premake5 clean
rm -rf bin bin-int build
echo "[SUCCESS] Clean complete."