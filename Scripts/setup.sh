#!/bin/bash

pushd ../vendor/vcpkg > /dev/null

chmod +x bootstrap-vcpkg.sh
./bootstrap-vcpkg.sh

./vcpkg install lua
./vcpkg install sol2
./vcpkg install spdlog
./vcpkg install doctest

popd > /dev/null