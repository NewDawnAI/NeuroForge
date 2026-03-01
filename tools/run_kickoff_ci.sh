#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build}"
BASELINE_PATH="${2:-$ROOT_DIR/.ci/warnings-baseline.json}"
BUILD_LOG="${3:-$ROOT_DIR/.ci/build.log}"

mkdir -p "$(dirname "$BASELINE_PATH")"
mkdir -p "$(dirname "$BUILD_LOG")"

echo "[kickoff-ci] configure"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR"

echo "[kickoff-ci] build (capturing log)"
cmake --build "$BUILD_DIR" --clean-first -j 2 2>&1 | tee "$BUILD_LOG"

if [[ ! -f "$BASELINE_PATH" ]]; then
  echo "[kickoff-ci] baseline missing; creating $BASELINE_PATH"
  python "$ROOT_DIR/tools/check_warning_regression.py" --build-log "$BUILD_LOG" --write-baseline "$BASELINE_PATH"
fi

echo "[kickoff-ci] warning regression check"
python "$ROOT_DIR/tools/check_warning_regression.py" --build-log "$BUILD_LOG" --baseline "$BASELINE_PATH"

echo "[kickoff-ci] deterministic smoke"
python "$ROOT_DIR/tools/ci_deterministic_smoke.py" --exe "$BUILD_DIR/neuroforge" --seed 123 --steps 5 --step-ms 1 --fail-on-missing-exe --allow-mismatch

echo "[kickoff-ci] done"
