#!/usr/bin/env bash
# benchmark.sh — Run a headless benchmark of the simulation.
# Usage: bash benchmark.sh [seconds] [output_file]
#   seconds     How long to run (default: 30)
#   output_file Where to write the report (default: benchmark_report.txt)
#
# After each run, a one-line summary is appended to benchmark_history.csv
# so performance can be tracked over time across commits.

set -euo pipefail

SECS="${1:-30}"
OUT="${2:-benchmark_report.txt}"
BIN="./build/ReachingUniversalis"
CSV="benchmark_history.csv"

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

# --- Append summary to benchmark_history.csv ---
if [ -f "$OUT" ]; then
    # Create CSV header if file doesn't exist
    if [ ! -f "$CSV" ]; then
        echo "date,git_hash,duration_s,avg_steps_s,final_day,total_deaths,avg_pop,min_pop,max_pop,avg_gold,avg_npc_wealth,gini" > "$CSV"
    fi

    # Parse fields from the report
    DATE=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    GIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
    DURATION=$(grep -oP 'Duration:\s+\K[0-9.]+' "$OUT" || echo "0")
    AVG_STEPS=$(grep -oP 'Avg steps/sec:\s+\K[0-9]+' "$OUT" || echo "0")
    FINAL_DAY=$(grep -oP 'Final game day:\s+\K[0-9]+' "$OUT" || echo "0")
    TOTAL_DEATHS=$(grep -oP 'Total deaths:\s+\K[0-9]+' "$OUT" || echo "0")
    AVG_POP=$(grep -oP 'Average:\s+\K[0-9.]+' "$OUT" || echo "0")
    MIN_POP=$(grep -oP 'Min:\s+\K[0-9]+' "$OUT" || echo "0")
    MAX_POP=$(grep -oP 'Max:\s+\K[0-9]+' "$OUT" || echo "0")
    AVG_GOLD=$(grep -oP 'Avg total gold:\s+\K[0-9]+' "$OUT" || echo "0")
    AVG_WEALTH=$(grep -oP 'Avg NPC wealth:\s+\K[0-9.]+' "$OUT" || echo "0")
    GINI=$(grep -oP 'Gini approx:\s+\K[0-9.]+' "$OUT" || echo "0")

    echo "${DATE},${GIT_HASH},${DURATION},${AVG_STEPS},${FINAL_DAY},${TOTAL_DEATHS},${AVG_POP},${MIN_POP},${MAX_POP},${AVG_GOLD},${AVG_WEALTH},${GINI}" >> "$CSV"

    echo ""
    echo "=== History appended to ${CSV} ==="
    echo "  Date: ${DATE}  Commit: ${GIT_HASH}  Duration: ${DURATION}s"
    echo "  Steps/s: ${AVG_STEPS}  Pop: ${AVG_POP}  Day: ${FINAL_DAY}  Deaths: ${TOTAL_DEATHS}  Gini: ${GINI}"
fi
