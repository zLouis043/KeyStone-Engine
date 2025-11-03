cd ../vendor/vcpkg/ 
call ./bootstrap-vcpkg.bat
vcpkg.exe install lua
vcpkg.exe install sol2
vcpkg.exe install spdlog
