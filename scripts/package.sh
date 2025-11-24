#!/bin/bash

set -e

# Build first
./scripts/build.sh

cd build

# Use linuxdeploy to create AppImage
# Assume linuxdeploy is installed
linuxdeploy --appdir AppDir --executable commit --desktop-file ../resources/commit.desktop --icon-file ../resources/commit.png

# Create AppImage
appimagetool AppDir

echo "AppImage created: commit-x86_64.AppImage"