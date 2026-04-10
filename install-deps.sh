#!/usr/bin/env bash
# install-deps.sh — install all build dependencies for ReachingUniversalis
# Run once: bash install-deps.sh
# Requires sudo.

set -euo pipefail

echo "==> Updating package index..."
sudo apt-get update -q

echo "==> Installing C++ build tools..."
sudo apt-get install -y \
    cmake \
    build-essential \
    g++ \
    make \
    git

echo "==> Installing Raylib (X11 + OpenGL + audio) dependencies..."
sudo apt-get install -y \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libxext-dev \
    libgl-dev \
    mesa-common-dev \
    libasound2-dev

echo ""
echo "All dependencies installed."
cmake --version
g++ --version | head -1
