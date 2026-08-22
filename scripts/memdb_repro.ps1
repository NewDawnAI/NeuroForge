# One-shot MemoryDB reproduction script
# - Creates/cleans a DB
# - Runs a short headless session to trigger periodic MemoryDB logging
# - Lists runs, episodes, and recent rewards for quick verification

[CmdletBinding()]
param(
    [string]$Database = "test_debug.sqlite",
    [int]$Steps = 200,
    [int]$StepMs = 5,
    [switch]$MemdbDebug,
    [switch]$RwciCanonical,
    [string]$ArtifactsDir = "Artifacts\\RWCI_Canonical_Run",
    [string]$SandboxUrl = "https://www.youtube.com/watch?v=aqz-KE-bpKQ"
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Find-NeuroforgeExe {
    param([string]$Base)
    $candidates = @(
        (Join-Path $Base 'build-vcpkg-msvc\Release\neuroforge.exe'),
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

if ($RwciCanonical) {
    $artifactsPath = if ([System.IO.Path]::IsPathRooted($ArtifactsDir)) { $ArtifactsDir } else { (Join-Path $repoRoot $ArtifactsDir) }
    New-Item -ItemType Directory -Force -Path $artifactsPath | Out-Null

    $telemetryPath = Join-Path $artifactsPath 'telemetry.sqlite'
    $tracePath = Join-Path $artifactsPath 'rwci_trace.jsonl'
    $consolePath = Join-Path $artifactsPath 'console.log'
    $exitPath = Join-Path $artifactsPath 'exit_status.txt'
    $commandPath = Join-Path $artifactsPath 'command.txt'
    $autonomyBeforePath = Join-Path $artifactsPath 'autonomy_envelope_before.json'
    $autonomyAfterPath = Join-Path $artifactsPath 'autonomy_envelope_after.json'

    foreach ($p in @($telemetryPath, $tracePath, $consolePath, $exitPath, $autonomyBeforePath, $autonomyAfterPath)) {
        if (Test-Path $p) { Remove-Item -Force -ErrorAction SilentlyContinue $p }
    }

    $env:NF_ASSERT_ENGINE_DB = "1"
    $env:NF_MEMDB_INTERVAL_MS = "1000"

    $runArgs = @(
        "--rwci=on",
        "--stagec=on",
        "--phase10=on",
        "--phase11=on",
        "--phase13=on",
        "--memdb-interval=1000",
        "--memory-db=`"$telemetryPath`"",
        "--log-json=`"$tracePath`"",
        "--sandbox=on",
        "--sandbox-url=$SandboxUrl",
        "--youtube-mode=on",
        "--steps=$Steps",
        "--step-ms=$StepMs"
    )

    $cmdLine = "`"$exe`" " + ($runArgs -join ' ')
    [System.IO.File]::WriteAllText($commandPath, $cmdLine, [System.Text.Encoding]::UTF8)
    Write-Host "Running canonical RWCI: $cmdLine"

    $exitCode = -1
    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $exe
        $psi.WorkingDirectory = $repoRoot
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.CreateNoWindow = $true
        $psi.Arguments = ($runArgs -join ' ')

        $proc = New-Object System.Diagnostics.Process
        $proc.StartInfo = $psi

        $null = $proc.Start()

        $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
        $stderrTask = $proc.StandardError.ReadToEndAsync()

        $proc.WaitForExit()

        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()

        [System.IO.File]::WriteAllText($consolePath, ($stdout + $stderr), [System.Text.Encoding]::UTF8)
        $exitCode = $proc.ExitCode
    } finally {
        [System.IO.File]::WriteAllText($exitPath, "$exitCode", [System.Text.Encoding]::UTF8)
    }

    $py = @'
import json
import sqlite3
import sys

db_path = sys.argv[1]
out_before = sys.argv[2]
out_after = sys.argv[3]

con = sqlite3.connect(db_path)
cur = con.cursor()

row = cur.execute("select id from runs order by id desc limit 1").fetchone()
run_id = row[0] if row else None

def fetch_one(order):
    if run_id is None:
        return None
    r = cur.execute(
        f"select id, run_id, ts_ms, decision, driver_json from autonomy_envelope_log where run_id=? order by id {order} limit 1",
        (run_id,),
    ).fetchone()
    if r is None:
        return None
    return {
        "id": r[0],
        "run_id": r[1],
        "ts_ms": r[2],
        "decision": r[3],
        "driver_json": json.loads(r[4]) if r[4] else None,
    }

before = fetch_one("asc")
after = fetch_one("desc")

with open(out_before, "w", encoding="utf-8") as f:
    json.dump({"run_id": run_id, "first": before}, f, indent=2)

with open(out_after, "w", encoding="utf-8") as f:
    json.dump({"run_id": run_id, "last": after}, f, indent=2)
'@

    try {
        $pyPath = Join-Path $artifactsPath "_dump_autonomy_tmp.py"
        [System.IO.File]::WriteAllText($pyPath, $py, [System.Text.Encoding]::UTF8)
        python $pyPath $telemetryPath $autonomyBeforePath $autonomyAfterPath | Out-Null
        try { [System.IO.File]::Delete($pyPath) } catch {}
    } catch {
        Write-Host "Warning: failed to dump autonomy snapshots: $($_.Exception.Message)"
    }

    if ($exitCode -ne 0) { throw "neuroforge run failed with exit code $exitCode" }
    Write-Host "Canonical run complete. Artifacts: $artifactsPath"
    exit 0
}

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
