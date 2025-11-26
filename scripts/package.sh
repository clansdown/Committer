#!/bin/bash

set -e

# Build first
./scripts/build.sh

cd build

# Create package directory and venv
mkdir -p ../package
uv venv ../package/.venv

# Install appimage-builder in the venv
uv pip install appimage-builder

# Run appimage-builder using uv run with the venv's Python
uv run --python ../package/.venv/bin/python appimage-builder --recipe ../appimage-builder.yml

echo "AppImage created"