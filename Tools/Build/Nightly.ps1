<#
.SYNOPSIS
    Nightly build + performance run for Web of the City (Phase 0 harness).

.DESCRIPTION
    1. Optionally pulls the latest commit (-Pull).
    2. Builds, cooks and packages a Development Win64 build with RunUAT BuildCookRun.
    3. Launches the packaged game on the perf map with -PerfRoute=<Route> -PerfRouteQuit.
    4. Runs Tools/Perf/analyze_perf.py against Section 4.3 budgets and this PC's baseline.
       The first run on a PC has no baseline, so it records one.

    Everything for a run lands in <OutputDir>\<timestamp>\ (log, package, CSV, perf_report.md).
    Exit codes: 0 = build OK and all perf gates pass, 1 = perf gate failed or regressed,
    2 = build failed, 3 = perf run failed to produce a CSV.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Tools\Build\Nightly.ps1

.EXAMPLE
    # Re-run perf on the last package without rebuilding, and accept the result as the new baseline.
    powershell -ExecutionPolicy Bypass -File Tools\Build\Nightly.ps1 -SkipBuild -WriteBaseline
#>
param(
    [string]$EngineDir = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.8" }),
    [string]$OutputDir = (Join-Path $env:USERPROFILE "WebOfTheCityBuilds"),
    [string]$Map = "/Game/WebOfTheCity/Maps/Test/L_PerfRoute_Swing",
    [string]$Route = "Swing",
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [int]$PerfTimeoutMinutes = 15,
    [switch]$Pull,
    [switch]$SkipBuild,
    [switch]$SkipPerf,
    [switch]$WriteBaseline
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Project = Join-Path $RepoRoot "WebOfTheCity.uproject"
$RunDir = Join-Path $OutputDir (Get-Date -Format "yyyyMMdd_HHmmss")
$LatestPackage = Join-Path $OutputDir "LatestPackage"
$LogFile = Join-Path $RunDir "nightly.log"
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null

function Write-Log([string]$Message) {
    $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $Message
    Write-Host $line
    Add-Content -Path $LogFile -Value $line
}

Write-Log "Web of the City nightly: repo $RepoRoot, engine $EngineDir, output $RunDir"

# ---- 1. Update ----------------------------------------------------------------
if ($Pull) {
    Write-Log "git pull --ff-only"
    & git -C $RepoRoot pull --ff-only
    if ($LASTEXITCODE -ne 0) { Write-Log "git pull failed; building the current checkout." }
}
$commit = (& git -C $RepoRoot rev-parse --short HEAD)
Write-Log "Commit $commit"

# ---- 2. Build, cook, package --------------------------------------------------
if (-not $SkipBuild) {
    $uat = Join-Path $EngineDir "Engine\Build\BatchFiles\RunUAT.bat"
    if (-not (Test-Path $uat)) {
        Write-Log "RunUAT.bat not found at $uat. Pass -EngineDir or set the UE_ROOT environment variable."
        exit 2
    }

    $uatArgs = @(
        "BuildCookRun",
        "-project=$Project",
        "-noP4",
        "-platform=Win64",
        "-clientconfig=Development",
        "-build", "-cook", "-stage", "-pak", "-archive",
        "-map=$Map",
        "-archivedirectory=$LatestPackage",
        "-utf8output",
        "-unattended"
    )
    Write-Log "RunUAT $($uatArgs -join ' ')"
    & $uat @uatArgs | Tee-Object -FilePath (Join-Path $RunDir "uat.log")
    if ($LASTEXITCODE -ne 0) {
        Write-Log "BUILD FAILED (RunUAT exit code $LASTEXITCODE). See uat.log."
        exit 2
    }
    Write-Log "Build OK."
}

if ($SkipPerf) {
    Write-Log "Perf run skipped."
    exit 0
}

# ---- 3. Perf route ------------------------------------------------------------
$packageRoot = Join-Path $LatestPackage "Windows"
$gameExe = Join-Path $packageRoot "WebOfTheCity\Binaries\Win64\WebOfTheCity.exe"
if (-not (Test-Path $gameExe)) { $gameExe = Join-Path $packageRoot "WebOfTheCity.exe" }
if (-not (Test-Path $gameExe)) {
    Write-Log "No packaged game at $packageRoot. Run without -SkipBuild first."
    exit 3
}

$perfDir = Join-Path $packageRoot "WebOfTheCity\Saved\Perf"
$gameArgs = @($Map, "-PerfRoute=$Route", "-PerfRouteQuit", "-windowed", "-ResX=$ResX", "-ResY=$ResY", "-nosplash", "-log")
$perfStart = Get-Date
Write-Log "Perf run: $gameExe $($gameArgs -join ' ')"
$proc = Start-Process -FilePath $gameExe -ArgumentList $gameArgs -PassThru
if (-not $proc.WaitForExit($PerfTimeoutMinutes * 60 * 1000)) {
    $proc.Kill()
    Write-Log "Perf run timed out after $PerfTimeoutMinutes minutes. Is the route in the map and the map in the package?"
    exit 3
}

$csv = Get-ChildItem -Path $perfDir -Filter "PerfRoute_${Route}_*.csv" -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $perfStart } |
    Sort-Object LastWriteTime |
    Select-Object -Last 1
if (-not $csv) {
    Write-Log "No PerfRoute CSV written. Check the game log in $packageRoot\WebOfTheCity\Saved\Logs."
    exit 3
}
Copy-Item $csv.FullName $RunDir
Write-Log "Captured $($csv.Name)"

# ---- 4. Analyze ---------------------------------------------------------------
$py = Get-Command py -ErrorAction SilentlyContinue
if ($py) {
    $pyExe = $py.Source
    $pyArgs = @("-3")
} else {
    $pyExe = (Get-Command python -ErrorAction Stop).Source
    $pyArgs = @()
}

$baseline = Join-Path $RepoRoot "Tools\Perf\Baselines\$env:COMPUTERNAME\$Route.json"
$analyzeArgs = $pyArgs + @(
    (Join-Path $RepoRoot "Tools\Perf\analyze_perf.py"),
    $csv.FullName,
    "--baseline", $baseline,
    "--report", (Join-Path $RunDir "perf_report.md")
)
if ($WriteBaseline -or -not (Test-Path $baseline)) {
    $analyzeArgs += @("--write-baseline", $baseline)
    Write-Log "Recording baseline at $baseline (commit it)."
}

& $pyExe @analyzeArgs | Tee-Object -FilePath (Join-Path $RunDir "analyze.log")
$perfExit = $LASTEXITCODE

$verdict = if ($perfExit -eq 0) { "PASS" } else { "FAIL" }
Add-Content -Path (Join-Path $OutputDir "history.csv") -Value ("{0},{1},{2},{3}" -f (Get-Date -Format "s"), $commit, $Route, $verdict)
Write-Log "Perf verdict: $verdict. Report: $(Join-Path $RunDir 'perf_report.md')"

if ($perfExit -eq 0) { exit 0 }
exit 1
