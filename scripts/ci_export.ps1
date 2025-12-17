param(
    [string]$db = "build\production_memory.db",
    [int]$telemetryInterval = 500,
    [int]$rewardInterval = 1000,
    [int]$steps = 1000,
    [string]$exePath = ".\neuroforge.exe",
    [string]$outDir = "build\production_exports"
)

Write-Host "[CI] Running NeuroForge production substrate..." -ForegroundColor Cyan

# Ensure output directory exists
if (-not (Test-Path -Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

# Execute the substrate run
if (-not (Test-Path -Path $exePath)) {
    Write-Warning "Executable not found at '$exePath'. Attempting fallback to build-vcpkg-vs\\Release\\neuroforge.exe."
    $exePath = ".\build-vcpkg-vs\Release\neuroforge.exe"
}

if (-not (Test-Path -Path $exePath)) {
    Write-Error "NeuroForge executable not found. Please set -exePath or build the binary."
    exit 1
}

& $exePath --memory-db $db --memdb-interval $telemetryInterval --reward-interval $rewardInterval --steps $steps --log-json=on
$runExit = $LASTEXITCODE
if ($runExit -ne 0) {
    Write-Error "NeuroForge run failed with exit code $runExit"
    exit $runExit
}

Write-Host "[CI] Exporting telemetry to '$outDir'..." -ForegroundColor Cyan

# Prefer analysis exporter if present
$exportScript = ".\analysis\phase5\export_phasec_csv.py"
if (-not (Test-Path -Path $exportScript)) {
    # Fallback path under scripts if available
    $exportScript = ".\scripts\export_phasec_csv.py"
}

if (-not (Test-Path -Path $exportScript)) {
    Write-Error "Export script not found. Expected at analysis\\phase5\\export_phasec_csv.py or scripts\\export_phasec_csv.py."
    exit 1
}

python $exportScript $db --out-dir $outDir
$exportExit = $LASTEXITCODE
if ($exportExit -ne 0) {
    Write-Error "Export failed with exit code $exportExit"
    exit $exportExit
}

# Write run metadata to assist integrity/report scripts
$runMeta = @{ telemetry_interval_ms = $telemetryInterval; reward_interval_ms = $rewardInterval; steps = $steps; db = $db; exe = $exePath }
$runMetaJson = $runMeta | ConvertTo-Json -Depth 3
Set-Content -Path "$outDir\run_meta.json" -Value $runMetaJson -Encoding UTF8

Write-Host "[CI] Completed run and export successfully." -ForegroundColor Green

Write-Host "[CI] Running telemetry integrity check..." -ForegroundColor Cyan
# Run integrity against the exports directory so run_meta.json can be consumed
python .\scripts\check_telemetry_integrity.py "$outDir" --out "$outDir\integrity_report.json"
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Integrity check encountered issues (exit $LASTEXITCODE). Proceeding to report generation."
}

Write-Host "[CI] Generating production report..." -ForegroundColor Cyan
python .\scripts\generate_production_report.py --exports "$outDir" --out "$outDir\production_report.md"
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Production report generation failed (exit $LASTEXITCODE)."
} else {
    Write-Host "[CI] Production report available at '$outDir\production_report.md'" -ForegroundColor Green
}

# Align dashboard alias with Phase 17b couplings preview
Write-Host "[CI] Updating dashboard alias with couplings preview..." -ForegroundColor Cyan
python .\scripts\export_context_peers_compat.py --meta "$outDir\run_meta.json" --alias ".\pages\tags\runner\context_peer_stream.json"
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Alias update failed (exit $LASTEXITCODE)."
} else {
    Write-Host "[CI] Alias updated successfully." -ForegroundColor Green
}
