cd ../vendor/vcpkg/ 
call ./bootstrap-vcpkg.bat
./vcpkg install lua
./vcpkg install sol2
./vcpkg install spdlog
