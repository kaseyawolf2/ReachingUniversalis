#!/usr/bin/env bash
# test_season_validation.sh — Build and run the season threshold validation test.
#
# Verifies that WorldLoader::LoadSeasons() correctly warns when season
# thresholds are inverted (e.g. harsh_cold < mild_cold).  Uses the
# intentionally-broken tests/test_seasons_invalid.toml config file.
#
# Usage:  bash tests/test_season_validation.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "==> Building SeasonValidationTest..."
cmake -B "$PROJECT_DIR/build" -S "$PROJECT_DIR" -DCMAKE_BUILD_TYPE=Debug 2>&1
cmake --build "$PROJECT_DIR/build" --target SeasonValidationTest --parallel "$(nproc)" 2>&1

echo ""
echo "==> Running SeasonValidationTest..."
cd "$PROJECT_DIR"
./build/SeasonValidationTest
