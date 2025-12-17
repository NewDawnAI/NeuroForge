param(
  [string]$ExePath = "build\neuroforge.exe",
  [string]$DbPath = "web\phasec_mem.db",
  [int]$RunId,
  [string]$OutPath = "web\metacognition_export.json"
)

Write-Host "Running NeuroForge phases 6–15..."
& $ExePath \
  --phase6 --phase7 --phase8 --phase9 --phase10 --phase11 --phase12 --phase13 --phase14 --phase15 \
  --phase9-modulation=on --phase11-revision-interval=180000 --phase14-window=10 --phase15-window=50 \
  --memory-db=$DbPath --steps=1000 --log-json=on --viewer=off
if ($LASTEXITCODE -ne 0) { throw "neuroforge.exe run failed with exit code $LASTEXITCODE" }

Write-Host "Exporting unified telemetry..."
$args = @("--db", $DbPath, "--out", $OutPath)
if ($PSBoundParameters.ContainsKey('RunId')) { $args += @("--run-id", $RunId) }
python .\scripts\dump_metacognition.py @args
if ($LASTEXITCODE -ne 0) { throw "export failed with exit code $LASTEXITCODE" }

Write-Host "Done. Open the dashboard:"
Write-Host " - If serving from web/: http://localhost:8000/phase9.html"
Write-Host " - If serving from repo root: http://localhost:8000/web/phase9.html"