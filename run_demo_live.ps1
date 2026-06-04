param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build",
    [string]$ConfigFile = "config_demo_live.json",
    [switch]$TestOnly,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $RepoRoot

function Write-Info([string]$Message) { Write-Host "[INFO] $Message" }
function Write-Err([string]$Message) { Write-Host "[ERR ] $Message" -ForegroundColor Red }

if (-not $env:HTTP_PROXY) { $env:HTTP_PROXY = "http://127.0.0.1:7897" }
if (-not $env:HTTPS_PROXY) { $env:HTTPS_PROXY = "http://127.0.0.1:7897" }
Write-Info "Proxy: HTTP_PROXY=$env:HTTP_PROXY"

$srcConfig = Join-Path $RepoRoot $ConfigFile
$dstConfig = Join-Path $RepoRoot "config.json"
if (-not (Test-Path $srcConfig)) {
    Write-Err "Config not found: $srcConfig"
    exit 1
}
Copy-Item -Force $srcConfig $dstConfig
Write-Info "Using config: $ConfigFile -> config.json"

$exe = Join-Path $RepoRoot "$BuildDir\$Configuration\Copilot_Coin.exe"
$testExe = Join-Path $RepoRoot "$BuildDir\$Configuration\test_demo_api.exe"

if (-not $SkipBuild) {
    cmake --build $BuildDir --config $Configuration 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (-not (Test-Path $testExe)) {
    cmake --build $BuildDir --config $Configuration --target test_demo_api 2>&1 | Out-Host
}

Write-Info "Step 1: API connectivity (demo-fapi.binance.com)"
& $testExe
if ($LASTEXITCODE -ne 0) {
    Write-Err "API test failed. Create API key at https://demo.binance.com (Futures enabled)."
    exit $LASTEXITCODE
}

if ($TestOnly) {
    Write-Info "TestOnly — skipping live trading loop."
    exit 0
}

Write-Info "Step 2: Start demo live trading (Ctrl+C to stop)"
Write-Info "Wallet UI: https://demo.binance.com/en/my/wallet/account/futures"
& $exe
