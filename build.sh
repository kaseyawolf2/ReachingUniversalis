#!/usr/bin/env bash
# build.sh — configure (first run) and build ReachingUniversalis
# Usage:
#   bash build.sh          # build (Debug by default)
#   bash build.sh release  # build Release
#   bash build.sh clean    # wipe build dir and rebuild

set -euo pipefail

BUILD_DIR="build"
BUILD_TYPE="Debug"

if [[ "${1:-}" == "release" ]]; then
    BUILD_TYPE="Release"
elif [[ "${1:-}" == "clean" ]]; then
    echo "==> Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

echo "==> Configuring ($BUILD_TYPE)..."
cmake \
    -B "$BUILD_DIR" \
    -S . \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "==> Building..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo ""
echo "Build complete: $BUILD_DIR/ReachingUniversalis"
