@echo off
echo [KeyStone] Cleaning Build Artifacts...
pushd ..\..\KeyStoneEngine
premake5 clean
popd

echo [KeyStone] Removing .vs and temp folders...
if exist "..\..\KeyStoneEngine\.vs" rd /s /q "..\..\KeyStoneEngine\.vs"
if exist "..\..\KeyStoneEngine\bin" rd /s /q "..\..\KeyStoneEngine\bin"
if exist "..\..\KeyStoneEngine\bin-int" rd /s /q "..\..\KeyStoneEngine\bin-int"
if exist "..\..\KeyStoneEngine\build" rd /s /q "..\..\KeyStoneEngine\build"

echo [SUCCESS] Clean complete.
pause