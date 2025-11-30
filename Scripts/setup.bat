@echo off
rem
pushd ..\vendor\vcpkg

rem
call bootstrap-vcpkg.bat

rem
vcpkg.exe install lua:x64-windows-static
vcpkg.exe install sol2:x64-windows-static
vcpkg.exe install spdlog:x64-windows-static
vcpkg.exe install doctest:x64-windows-static

rem
popd
pause