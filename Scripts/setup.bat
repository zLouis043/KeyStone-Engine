cd ../vendor/vcpkg/ 
call ./bootstrap-vcpkg.bat
call ./vcpkg install lua
call ./vcpkg install sol2
call ./vcpkg install spdlog
call ./vcpkg install doctest
