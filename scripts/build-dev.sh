#!/bin/bash

set -e

mkdir -p build
mkdir -p bin
cd build
cmake ..
make dev -j$(nproc)

echo "Dev build completed. Executable is in bin/commit"