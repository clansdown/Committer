#!/bin/bash

set -e

mkdir -p build
mkdir -p bin
cd build
cmake ..
make -j$(nproc)

echo "Build completed. Executable is in bin/commit"