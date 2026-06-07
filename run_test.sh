#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_ROOT"

# Defaults
CSV_PATH="${1:-data/BTCUSDT_1h.csv}"
BUILD_TYPE="${2:-Release}"
BUILD_DIR="build"
REPORT=false
SKIP_BUILD=false

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --csv)        CSV_PATH="$2"; shift 2 ;;
        --release)    BUILD_TYPE="Release"; shift ;;
        --debug)      BUILD_TYPE="Debug"; shift ;;
        --report)     REPORT=true; shift ;;
        --skip-build) SKIP_BUILD=true; shift ;;
        --clean)
            rm -rf "$REPO_ROOT/reports"
            shift ;;
        *)            shift ;;
    esac
done

info()  { echo "[INFO] $*"; }
warn()  { echo "[WARN] $*" >&2; }
err()   { echo "[ERR ] $*" >&2; }

# Clean reports if requested
if [ "$REPORT" = true ] && [ -d "$REPO_ROOT/reports" ]; then
    info "Cleaning reports folder"
    rm -rf "$REPO_ROOT/reports"
fi

# Build
if [ "$SKIP_BUILD" = false ]; then
    info "Building ($BUILD_TYPE)..."
    cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j$(nproc)
fi

EXE="$BUILD_DIR/Copilot_Coin"
if [ ! -f "$EXE" ]; then
    err "Executable not found: $EXE"
    exit 1
fi

# Check CSV path
if [ ! -f "$CSV_PATH" ]; then
    err "CSV file not found: $CSV_PATH"
    exit 1
fi

info "Using build type: $BUILD_TYPE"
info "Using executable: $EXE"
info "Using CSV: $CSV_PATH"

# Build backtest args
BT_ARGS="backtest"
if [ "$REPORT" = true ]; then
    BT_ARGS="$BT_ARGS report"
else
    BT_ARGS="$BT_ARGS offline"
fi
BT_ARGS="$BT_ARGS --csvPath $CSV_PATH"

# Run backtest
info "Running backtest: $BT_ARGS"
"$EXE" $BT_ARGS

info "Done. Check the reports folder for output files."
SUMMARY_JSON="$REPO_ROOT/reports/bt_summary.json"
EQUITY_CSV="$REPO_ROOT/reports/bt_equity.csv"
if [ -f "$SUMMARY_JSON" ]; then info "Summary: $SUMMARY_JSON"; fi
if [ -f "$EQUITY_CSV" ]; then info "Equity:  $EQUITY_CSV"; fi