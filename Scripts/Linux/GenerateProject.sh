#!/bin/bash
echo "[KeyStone] Generating Makefiles..."
cd ../../KeyStoneEngine
premake5 gmake2
echo "Done. Run 'make config=debug_x64' to build."