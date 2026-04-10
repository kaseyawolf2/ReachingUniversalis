#!/usr/bin/env bash
# test.sh — run the game headlessly via Xvfb and report whether it survives
# Usage: bash test.sh [seconds]   (default: 5 seconds)

set -euo pipefail

BINARY="./build/ReachingUniversalis"
RUN_SECONDS="${1:-5}"

if [[ ! -f "$BINARY" ]]; then
    echo "ERROR: binary not found — run 'bash build.sh' first"
    exit 1
fi

if ! command -v xvfb-run &>/dev/null; then
    echo "==> Installing Xvfb..."
    sudo apt-get install -y xvfb
fi

echo "==> Running for ${RUN_SECONDS}s under virtual display..."
xvfb-run --auto-servernum --server-args="-screen 0 1280x720x24" \
    timeout "$RUN_SECONDS" "$BINARY" && EXIT=$? || EXIT=$?

if [[ $EXIT -eq 124 || $EXIT -eq 0 ]]; then
    echo "PASS: game ran for ${RUN_SECONDS}s without crashing (exit $EXIT)"
    exit 0
else
    echo "FAIL: game exited with code $EXIT before ${RUN_SECONDS}s elapsed"
    exit 1
fi
