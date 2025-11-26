#!/bin/bash

set -e

# Build first
./scripts/build.sh

cd build

# Create package directory and venv
mkdir -p ../package
uv venv ../package/.venv
source ../package/.venv/bin/activate

# Install and run appimage-builder in the venv
uv pip install appimage-builder
appimage-builder --recipe ../appimage-builder.yml

# Optional: deactivate venv
deactivate

echo "AppImage created"