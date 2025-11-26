#!/bin/bash

set -e

# Build first
./scripts/build.sh

# Run snapcraft
snapcraft

echo "Snap created"