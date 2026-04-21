#!/usr/bin/env bash
# run_all_tests.sh — Build, run all unit tests, smoke test, and optionally benchmark.
#
# Usage:
#   bash run_all_tests.sh                 # tests only (debug build)
#   bash run_all_tests.sh --benchmark     # tests + 60s benchmark (debug + release build)
#   bash run_all_tests.sh --benchmark 30  # tests + 30s benchmark
#
# Exit code: number of failures (0 = all passed)

set -uo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

FAILURES=0
DO_BENCHMARK=false
BENCH_SECS=60

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        --benchmark)
            DO_BENCHMARK=true
            if [[ "${2:-}" =~ ^[0-9]+$ ]]; then
                BENCH_SECS="$2"
                shift
            fi
            shift
            ;;
        *)
            shift
            ;;
    esac
done

pass() { echo -e "  ${GREEN}PASS${NC}  $1"; }
fail() { echo -e "  ${RED}FAIL${NC}  $1"; FAILURES=$((FAILURES + 1)); }
section() { echo -e "\n${BOLD}== $1 ==${NC}"; }

# ---- 1. Debug Build ----
section "Debug Build"
if bash build.sh clean 2>&1 | tail -5; then
    pass "Debug build"
else
    fail "Debug build"
fi

# ---- 2. Unit Tests ----
section "Unit Tests"

# CMake targets
for TEST_BIN in DynBitsetTest BuildMapOrderingTest SeasonValidationTest; do
    if [[ -f "./build/$TEST_BIN" ]]; then
        if OUTPUT=$(./build/"$TEST_BIN" 2>&1); then
            # Count pass/fail lines if present
            NPASSED=$(echo "$OUTPUT" | grep -c "PASS" || true)
            pass "$TEST_BIN ($NPASSED assertions passed)"
        else
            fail "$TEST_BIN"
            echo "$OUTPUT" | tail -10
        fi
    else
        fail "$TEST_BIN (binary not found)"
    fi
done

# Header-only test (SkillRateTest — not a CMake target)
if [[ -f "tests/SkillRateTest.cpp" ]]; then
    if g++ -std=c++17 -I src tests/SkillRateTest.cpp -o build/SkillRateTest 2>&1; then
        if OUTPUT=$(./build/SkillRateTest 2>&1); then
            NPASSED=$(echo "$OUTPUT" | grep -c "PASS" || true)
            pass "SkillRateTest ($NPASSED assertions passed)"
        else
            fail "SkillRateTest"
            echo "$OUTPUT" | tail -10
        fi
    else
        fail "SkillRateTest (compilation failed)"
    fi
fi

# Auto-discover any new *Test.cpp files not covered above
KNOWN_TESTS="DynBitsetTest BuildMapOrderingTest SeasonValidationTest SkillRateTest"
for TEST_SRC in tests/*Test*.cpp; do
    [[ -f "$TEST_SRC" ]] || continue
    BASE=$(basename "$TEST_SRC" .cpp)
    if echo "$KNOWN_TESTS" | grep -qw "$BASE"; then
        continue  # already handled
    fi
    echo -e "  ${YELLOW}NEW${NC}   Discovered unknown test: $BASE — attempting build..."
    if g++ -std=c++17 -I src "$TEST_SRC" -o "build/$BASE" 2>&1; then
        if OUTPUT=$("./build/$BASE" 2>&1); then
            pass "$BASE (auto-discovered)"
        else
            fail "$BASE (auto-discovered)"
            echo "$OUTPUT" | tail -10
        fi
    else
        echo -e "  ${YELLOW}SKIP${NC}  $BASE — could not compile standalone (may need CMake)"
    fi
done

# ---- 3. Smoke Test ----
section "Smoke Test"

if ! command -v xvfb-run &>/dev/null; then
    echo "  Installing Xvfb..."
    sudo apt-get install -y xvfb >/dev/null 2>&1
fi

SMOKE_SECS=10
if xvfb-run --auto-servernum --server-args="-screen 0 1280x720x24" \
    timeout "$SMOKE_SECS" ./build/ReachingUniversalis 2>&1 && EXIT=$? || EXIT=$?; then
    true
fi
if [[ $EXIT -eq 124 || $EXIT -eq 0 ]]; then
    pass "Smoke test (ran ${SMOKE_SECS}s, exit $EXIT)"
else
    fail "Smoke test (crashed with exit $EXIT after <${SMOKE_SECS}s)"
fi

# ---- 4. Benchmark (optional) ----
if $DO_BENCHMARK; then
    section "Release Build"
    if bash build.sh release 2>&1 | tail -5; then
        pass "Release build"
    else
        fail "Release build"
    fi

    section "Benchmark (${BENCH_SECS}s)"
    REPORT="benchmark_report.txt"
    if xvfb-run --auto-servernum --server-args="-screen 0 1280x720x24" \
        ./build/ReachingUniversalis --benchmark "$BENCH_SECS" "$REPORT" 2>&1; then
        pass "Benchmark completed"
        echo ""
        cat "$REPORT"
    else
        fail "Benchmark crashed"
    fi

    # Baseline comparison
    if [[ -f "benchmark_baseline.txt" ]]; then
        section "Baseline Comparison"
        # Extract key metrics from both files
        baseline_sps=$(grep "Avg steps/sec" benchmark_baseline.txt | awk '{print $NF}' || echo "0")
        current_sps=$(grep "Avg steps/sec" "$REPORT" | awk '{print $NF}' || echo "0")
        baseline_pop=$(grep "^Average:" benchmark_baseline.txt | awk '{print $NF}' || echo "0")
        current_pop=$(grep "^Average:" "$REPORT" | awk '{print $NF}' || echo "0")

        echo "  Steps/sec:  baseline=$baseline_sps  current=$current_sps"
        echo "  Avg pop:    baseline=$baseline_pop  current=$current_pop"

        # Flag large changes via awk
        echo "$baseline_sps $current_sps" | awk '{
            if ($1 > 0) {
                pct = (($2 - $1) / $1) * 100;
                if (pct < -10) printf "  \033[0;31mWARNING: steps/sec dropped %.1f%%\033[0m\n", pct;
                else if (pct > 10) printf "  \033[0;32msteps/sec improved %.1f%%\033[0m\n", pct;
                else printf "  steps/sec change: %.1f%% (within tolerance)\n", pct;
            }
        }'
    else
        echo ""
        echo "  No benchmark_baseline.txt found — skipping comparison."
        echo "  To set a baseline: cp benchmark_report.txt benchmark_baseline.txt"
    fi
fi

# ---- Summary ----
section "Summary"
if [[ $FAILURES -eq 0 ]]; then
    echo -e "  ${GREEN}${BOLD}ALL CLEAR${NC} — all tests passed"
else
    echo -e "  ${RED}${BOLD}$FAILURES FAILURE(S)${NC}"
fi

exit $FAILURES
