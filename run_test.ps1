param(
    [string]$CsvPath = "F:\Project\test\Copilot_Coin\data\BTCUSDT_1h.csv",
    [string]$Configuration = "Debug",
    [string]$BuildDir = "build",
    [switch]$SkipBuild,
    [switch]$Report,
    [switch]$OpenResults,
    [switch]$Release,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

if ($Release) {
    $Configuration = "Release"
}

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $RepoRoot

function Write-Info([string]$Message) {
    Write-Host "[INFO] $Message"
}

function Write-Warn([string]$Message) {
    Write-Host "[WARN] $Message" -ForegroundColor Yellow
}

function Write-Err([string]$Message) {
    Write-Host "[ERR ] $Message" -ForegroundColor Red
}

function Invoke-CommandChecked {
    param(
        [Parameter(Mandatory = $true)] [string]$FilePath,
        [Parameter(Mandatory = $true)] [string[]]$Arguments,
        [Parameter(Mandatory = $true)] [string]$StepName
    )

    Write-Info "${StepName}: $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "$StepName failed with exit code $exitCode. Check the output above for details."
    }
}

function Open-IfRequested {
    param(
        [Parameter(Mandatory = $true)] [string]$PathToOpen
    )

    if ($OpenResults -and (Test-Path $PathToOpen)) {
        try {
            Start-Process -FilePath $PathToOpen | Out-Null
            Write-Info "Opened: $PathToOpen"
        } catch {
            Write-Warn "Could not open: $PathToOpen"
        }
    }
}

function Find-Executable {
    param(
        [Parameter(Mandatory = $true)] [string]$Root,
        [Parameter(Mandatory = $true)] [string]$Config,
        [Parameter(Mandatory = $true)] [string]$BuildFolder
    )

    $candidates = @(
        (Join-Path $Root "$BuildFolder\$Config\Copilot_Coin.exe"),
        (Join-Path $Root "$BuildFolder\Release\Copilot_Coin.exe"),
        (Join-Path $Root "$BuildFolder\Debug\Copilot_Coin.exe"),
        (Join-Path $Root "$BuildFolder\Copilot_Coin.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return $candidate }
    }

    return $null
}

$reportsDir = Join-Path $RepoRoot "reports"
if ($Clean -and (Test-Path $reportsDir)) {
    Write-Info "Cleaning reports folder: $reportsDir"
    Remove-Item -Recurse -Force $reportsDir
}

$exePath = Find-Executable -Root $RepoRoot -Config $Configuration -BuildFolder $BuildDir

if (-not $SkipBuild) {
    $sln = Get-ChildItem -Path $RepoRoot -Filter *.sln -File -ErrorAction SilentlyContinue | Select-Object -First 1
    $cmakeLists = Join-Path $RepoRoot "CMakeLists.txt"

    if (-not $sln -and -not (Test-Path $cmakeLists)) {
        Write-Err "No build system file found (CMakeLists.txt / .sln)."
        Write-Err "Please add a build definition or use -SkipBuild if the executable already exists."
        exit 1
    }

    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    try {
        if ($sln) {
            Write-Info "Detected Visual Studio solution: $($sln.Name)"
            Invoke-CommandChecked -FilePath "cmake" -Arguments @("-S", ".", "-B", $BuildDir, "-A", "x64") -StepName "CMake configure"
            Invoke-CommandChecked -FilePath "cmake" -Arguments @("--build", $BuildDir, "--config", $Configuration) -StepName "CMake build"
        }
        else {
            Write-Info "Detected CMakeLists.txt"
            Invoke-CommandChecked -FilePath "cmake" -Arguments @("-S", ".", "-B", $BuildDir, "-A", "x64") -StepName "CMake configure"
            Invoke-CommandChecked -FilePath "cmake" -Arguments @("--build", $BuildDir, "--config", $Configuration) -StepName "CMake build"
        }
    } catch {
        Write-Err $_.Exception.Message
        Write-Err "Build failed. Common checks:"
        Write-Err "  - Confirm Visual Studio C++ workload is installed"
        Write-Err "  - Confirm CMake is installed and available in PATH"
        Write-Err "  - Confirm vcpkg dependencies are installed if required"
        exit 1
    }

    $exePath = Find-Executable -Root $RepoRoot -Config $Configuration -BuildFolder $BuildDir
}

if (-not $exePath) {
    Write-Err "Executable not found. Looked for these paths:"
    @(
        (Join-Path $RepoRoot "$BuildDir\$Configuration\Copilot_Coin.exe"),
        (Join-Path $RepoRoot "$BuildDir\Release\Copilot_Coin.exe"),
        (Join-Path $RepoRoot "$BuildDir\Debug\Copilot_Coin.exe"),
        (Join-Path $RepoRoot "$BuildDir\Copilot_Coin.exe")
    ) | ForEach-Object { Write-Err "  $_" }
    exit 1
}

if (-not (Test-Path $CsvPath)) {
    Write-Err "CSV file not found: $CsvPath"
    exit 1
}

Write-Info "Using configuration: $Configuration"
Write-Info "Using executable: $exePath"
Write-Info "Using CSV: $CsvPath"

$backtestArgs = @("backtest")
if ($Report) {
    $backtestArgs += "report"
} else {
    $backtestArgs += "offline"
}
$backtestArgs += "--csvPath"
$backtestArgs += $CsvPath

try {
    Invoke-CommandChecked -FilePath $exePath -Arguments $backtestArgs -StepName "Run backtest"
} catch {
    Write-Err $_.Exception.Message
    exit 1
}

$summaryJson = Join-Path $reportsDir "bt_summary.json"
$equityCsv = Join-Path $reportsDir "bt_equity.csv"

Write-Info "Done. Check the reports folder for output files."
if (Test-Path $summaryJson) { Write-Info "Summary: $summaryJson" }
if (Test-Path $equityCsv) { Write-Info "Equity:  $equityCsv" }

if ($OpenResults) {
    if (Test-Path $summaryJson) {
        Open-IfRequested -PathToOpen $summaryJson
    }
    elseif (Test-Path $equityCsv) {
        Open-IfRequested -PathToOpen $equityCsv
    }
    else {
        Write-Warn "No report file found to open."
    }
}
