@echo off
echo [KeyStone] Generating Visual Studio 2022 Solution...
pushd ..\..\KeyStoneEngine
premake5 vs2022
popd
pause