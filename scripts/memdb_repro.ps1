# One-shot MemoryDB reproduction script
# - Creates/cleans a DB
# - Runs a short headless session to trigger periodic MemoryDB logging
# - Lists runs, episodes, and recent rewards for quick verification

[CmdletBinding()]
param(
    [string]$Database = "test_debug.sqlite",
    [int]$Steps = 200,
    [int]$StepMs = 5,
    [switch]$MemdbDebug
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Find-NeuroforgeExe {
    param([string]$Base)
    $candidates = @(
        (Join-Path $Base 'build\Release\neuroforge.exe'),
        (Join-Path $Base 'build\Debug\neuroforge.exe'),
        (Join-Path $Base 'build\neuroforge.exe'),
        (Join-Path $Base 'Debug\neuroforge.exe'),
        (Join-Path $Base 'Release\neuroforge.exe'),
        (Join-Path $Base 'build-vs\Debug\neuroforge.exe'),
        (Join-Path $Base 'build-vs\Release\neuroforge.exe'),
        (Join-Path $Base 'build-vcpkg-rel\Release\neuroforge.exe'),
        (Join-Path $Base 'build-vcpkg-rel\Debug\neuroforge.exe')
    )
    foreach ($p in $candidates) { if (Test-Path $p) { return $p } }
    throw "neuroforge.exe not found. Checked: $($candidates -join ', ')"
}

# Determine repo root (one level above scripts directory)
$scriptDir = Split-Path -Parent $PSCommandPath
$repoRoot = Split-Path -Parent $scriptDir

$exe = Find-NeuroforgeExe -Base $repoRoot
Write-Host "Using neuroforge: $exe"

# Clean DB at repo root for consistency with other tools
$dbPath = Join-Path $repoRoot $Database
if (Test-Path $dbPath) { Remove-Item -Force -ErrorAction SilentlyContinue $dbPath }

# Run short session
$memdbDebug = if ($MemdbDebug) { 'on' } else { 'off' }
$runArgs = @(
    "--memory-db=`"$dbPath`"",
    "--memdb-debug=$memdbDebug",
    "--steps=$Steps",
    "--step-ms=$StepMs",
    "--vision-demo=off"
)
Write-Host "Running session: $($runArgs -join ' ')"
$proc = Start-Process -FilePath $exe -ArgumentList $runArgs -NoNewWindow -PassThru -Wait
if ($proc.ExitCode -ne 0) { throw "neuroforge run failed with exit code $($proc.ExitCode)" }

# List runs
$listArgs = @("--memory-db=`"$dbPath`"", "--list-runs")
$runsOut = & $exe @listArgs
Write-Host "`n-- Runs --`n$runsOut"

# Parse last run id
$lines = $runsOut -split "`r?`n" | Where-Object { $_ -and ($_ -notmatch '^Runs count=') }
if (-not $lines) { throw "No runs found in DB $dbPath" }
$last = $lines[-1]
$runId = ($last -split ',')[0]
if (-not $runId) { throw "Failed to parse run id from: $last" }
Write-Host "Selected run id: $runId"

# List episodes
$epsOut = & $exe --memory-db="`"$dbPath`"" --list-episodes=$runId
Write-Host "`n-- Episodes --`n$epsOut"

# List rewards (limit 10)
$rewOut = & $exe --memory-db="`"$dbPath`"" --recent-rewards="${runId},10"
Write-Host "`n-- Recent Rewards --`n$rewOut"

Write-Host "`nReproduction complete. DB: $dbPath"