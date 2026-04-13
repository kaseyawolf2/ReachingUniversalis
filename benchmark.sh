#!/usr/bin/env bash
# benchmark.sh — Run a headless benchmark of the simulation.
# Usage: bash benchmark.sh [seconds] [output_file]
#   seconds     How long to run (default: 30)
#   output_file Where to write the report (default: benchmark_report.txt)

set -euo pipefail

SECS="${1:-30}"
OUT="${2:-benchmark_report.txt}"
BIN="./build/ReachingUniversalis"

if [ ! -f "$BIN" ]; then
    echo "Binary not found. Building..."
    bash build.sh
fi

# Try to run headlessly via Xvfb if no display is available
if [ -z "${DISPLAY:-}" ]; then
    if command -v xvfb-run &>/dev/null; then
        echo "No DISPLAY set — using xvfb-run"
        xvfb-run -a -s "-screen 0 1280x720x24" "$BIN" --benchmark "$SECS" "$OUT"
    elif command -v Xvfb &>/dev/null; then
        echo "No DISPLAY set — starting Xvfb manually"
        Xvfb :99 -screen 0 1280x720x24 &>/dev/null &
        XVFB_PID=$!
        trap "kill $XVFB_PID 2>/dev/null" EXIT
        sleep 0.5
        DISPLAY=:99 "$BIN" --benchmark "$SECS" "$OUT"
    else
        echo "ERROR: No DISPLAY and Xvfb not available."
        echo "Install xvfb (sudo apt install xvfb) or run with a display."
        exit 1
    fi
else
    "$BIN" --benchmark "$SECS" "$OUT"
fi

echo ""
echo "=== Benchmark complete ==="
if [ -f "$OUT" ]; then
    cat "$OUT"
fi
