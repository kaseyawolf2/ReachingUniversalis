#!/usr/bin/env bash
# screenshot.sh — launch the game on a virtual display, wait, take a screenshot, then kill it.
# Usage: bash screenshot.sh [wait_seconds] [output_path]
#   wait_seconds: how long to let the game run before capturing (default: 3)
#   output_path:  where to save the PNG (default: /tmp/game_screenshot.png)

set -euo pipefail

BINARY="./build/ReachingUniversalis"
WAIT="${1:-3}"
OUTPUT="${2:-/tmp/game_screenshot.png}"
DISPLAY_NUM=":99"

if [[ ! -f "$BINARY" ]]; then
    echo "ERROR: binary not found — run 'bash build.sh' first"
    exit 1
fi

for cmd in Xvfb import; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "ERROR: '$cmd' not found. Install with: sudo apt-get install -y xvfb imagemagick"
        exit 1
    fi
done

# Kill any leftover Xvfb on this display
pkill -f "Xvfb ${DISPLAY_NUM}" 2>/dev/null || true
sleep 0.2

# Start a persistent Xvfb so we can screenshot it
Xvfb ${DISPLAY_NUM} -screen 0 1280x720x24 &
XVFB_PID=$!
sleep 0.3

cleanup() {
    kill "$GAME_PID" 2>/dev/null || true
    kill "$XVFB_PID" 2>/dev/null || true
    wait "$GAME_PID" 2>/dev/null || true
    wait "$XVFB_PID" 2>/dev/null || true
}
trap cleanup EXIT

# Launch the game on that display
DISPLAY=${DISPLAY_NUM} "$BINARY" &
GAME_PID=$!

echo "==> Game launched (PID $GAME_PID), waiting ${WAIT}s for UI to render..."
sleep "$WAIT"

# Check the game is still alive
if ! kill -0 "$GAME_PID" 2>/dev/null; then
    echo "ERROR: game exited before screenshot could be taken"
    exit 1
fi

# Capture the full screen
DISPLAY=${DISPLAY_NUM} import -window root "$OUTPUT"
echo "==> Screenshot saved to $OUTPUT"
