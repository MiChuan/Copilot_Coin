# 币安 U 本位合约 API 端点连通性测试（支持 Demo / Testnet）
param(
    [string]$BaseUrl = "https://demo-fapi.binance.com",
    [string]$Proxy = "http://127.0.0.1:7897",
    [string]$Symbol = "BTCUSDT"
)

$ErrorActionPreference = "Stop"
if ($Proxy) {
    $env:HTTP_PROXY = $Proxy
    $env:HTTPS_PROXY = $Proxy
}

function Test-Endpoint {
    param([string]$Name, [string]$Uri, [switch]$Signed)
    Write-Host "`n[$Name]" -ForegroundColor Cyan
    Write-Host "  GET $Uri"
    try {
        if ($Signed) {
            Write-Host "  (跳过: 签名接口请在 test_demo_api.exe 中测试)" -ForegroundColor Yellow
            return
        }
        $r = Invoke-RestMethod -Uri $Uri -Method Get
        Write-Host "  OK" -ForegroundColor Green
        if ($r -is [array]) { Write-Host "  rows=$($r.Count)" }
        else { $r | ConvertTo-Json -Compress -Depth 3 | ForEach-Object { Write-Host "  $_" } }
    } catch {
        Write-Host "  FAIL: $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host "Base: $BaseUrl  Proxy: $Proxy  Symbol: $Symbol"

Test-Endpoint "/fapi/v1/time" "$BaseUrl/fapi/v1/time"
Test-Endpoint "/fapi/v1/exchangeInfo" "$BaseUrl/fapi/v1/exchangeInfo?symbol=$Symbol"
Test-Endpoint "/fapi/v1/ticker/price" "$BaseUrl/fapi/v1/ticker/price?symbol=$Symbol"
Test-Endpoint "/fapi/v1/depth" "$BaseUrl/fapi/v1/depth?symbol=$Symbol&limit=10"
Test-Endpoint "/fapi/v1/klines" "$BaseUrl/fapi/v1/klines?symbol=$Symbol&interval=1h&limit=10"

Write-Host "`n签名接口 (v2):" -ForegroundColor Cyan
Write-Host "  /fapi/v2/account  /fapi/v2/balance"
Write-Host "  运行: .\build\Release\test_demo_api.exe"
