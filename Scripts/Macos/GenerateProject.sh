#!/bin/bash
echo "[KeyStone] Generating Makefiles..."
cd ../../KeyStoneEngine
premake5 gmake2
echo "Done."