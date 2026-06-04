#!/bin/bash

echo "=== Testing Binance API Endpoints ==="

echo ""
echo "1. Testing Demo Futures API: https://demo-fapi.binance.com"
curl -s -m 30 https://demo-fapi.binance.com/fapi/v1/ping
echo ""

echo ""
echo "2. Testing Testnet Futures API: https://testnet.binancefuture.com"
curl -s -m 30 https://testnet.binancefuture.com/fapi/v1/ping
echo ""

echo ""
echo "3. Testing Mainnet Futures API: https://fapi.binance.com"
curl -s -m 30 https://fapi.binance.com/fapi/v1/ping
echo ""

echo ""
echo "=== Test Completed ==="
