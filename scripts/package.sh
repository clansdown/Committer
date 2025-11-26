#!/bin/bash

set -e

# Build first
./scripts/build.sh

# Create package directory and enter it
mkdir -p package
cd package

# Create venv
uv venv .venv

# Install appimage-builder
uv pip install appimage-builder

# Copy the recipe
cp ../appimage-builder.yml .

# Run appimage-builder
uv run appimage-builder --recipe appimage-builder.yml

echo "AppImage created in package/Committer-1.0.0-x86_64.AppImage"
