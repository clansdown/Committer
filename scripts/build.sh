#!/bin/bash

set -e

mkdir -p build
cd build
cmake ..
make -j$(nproc)

echo "Build completed. Executable is in build/commit"