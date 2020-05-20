#!/bin/bash

if [ "$#" -lt 1 ]; then
	BUILD_TYPE=Release
else
	BUILD_TYPE=$1
fi

BUILD_PATH=$(realpath ./.build/$BUILD_TYPE-Windows)
mkdir -p $BUILD_PATH
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_TOOLCHAIN_FILE=$(realpath ./CMake/MinGWToolchain.cmake) -DBUILD_ASSETGEN=OFF . -B$BUILD_PATH
