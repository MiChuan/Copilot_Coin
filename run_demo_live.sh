#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="build"
CONFIG_FILE="${1:-config_demo_live.json}"
BUILD_TYPE="${2:-Release}"

info()  { echo "[INFO] $*"; }
err()   { echo "[ERR ] $*" >&2; }

# Proxy setup (optional)
if [ -z "$HTTP_PROXY" ] && [ -z "$HTTPS_PROXY" ]; then
    export HTTP_PROXY="http://127.0.0.1:10808"
    export HTTPS_PROXY="http://127.0.0.1:10808"
fi
info "Proxy: HTTP_PROXY=$HTTP_PROXY  HTTPS_PROXY=$HTTPS_PROXY"

# Copy config
SRC_CONFIG="$REPO_ROOT/$CONFIG_FILE"
DST_CONFIG="$REPO_ROOT/config.json"
if [ ! -f "$SRC_CONFIG" ]; then
    err "Config not found: $SRC_CONFIG"
    exit 1
fi
cp -f "$SRC_CONFIG" "$DST_CONFIG"
info "Using config: $CONFIG_FILE -> config.json"

# Remove backtest flag if present
BT_FLAG="$REPO_ROOT/run_backtest.flag"
if [ -f "$BT_FLAG" ]; then
    rm -f "$BT_FLAG"
    info "Removed run_backtest.flag (forces backtest mode if present)"
fi

# Build
info "Building..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j$(nproc)

EXE="$BUILD_DIR/Copilot_Coin"
TEST_EXE="$BUILD_DIR/test_demo_api"

if [ ! -f "$TEST_EXE" ]; then
    cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --target test_demo_api -j$(nproc)
fi

# Step 1: API connectivity test
info "Step 1: API connectivity (demo-fapi.binance.com)"
"$TEST_EXE"
if [ $? -ne 0 ]; then
    err "API test failed. Create API key at https://demo.binance.com (Futures enabled)."
    exit 1
fi

# Check for --test-only flag
if [ "${3:-}" = "--test-only" ]; then
    info "TestOnly — skipping live trading loop."
    exit 0
fi

# Step 2: Start demo live trading
info "Step 2: Start demo live trading (Ctrl+C to stop)"
info "Wallet UI: https://demo.binance.com/en/my/wallet/account/futures"
"$EXE"