param(
  [int]$episodesTarget = 5,
  [string]$pattern = "phase6_m*.jsonl",
  [string]$summarizer = "tools/summarize_phase6_sweep.py",
  [string]$csvOut = "pages/tags/runner/phase6_sweep_summary.csv",
  [string]$jsonOut = "pages/tags/runner/phase6_sweep_summary.json",
  [int]$pollSeconds = 30
)

function GetEpisodeEndCount {
  param([string]$file)
  if (-not (Test-Path -Path $file)) { return 0 }
  try {
    $matches = Select-String -Path $file -Pattern '"event":"episode_end"' -AllMatches
    return $matches.Matches.Count
  } catch {
    return 0
  }
}

Write-Host "Waiting for sweep files ($pattern) to appear..."

# Ensure at least some files are present
while ((Get-ChildItem -File -Name $pattern -ErrorAction SilentlyContinue).Count -lt 1) {
  Start-Sleep -Seconds $pollSeconds
}

Write-Host "Monitoring episode_end counts until each file reaches $episodesTarget episodes..."

while ($true) {
  $files = Get-ChildItem -File -Name $pattern -ErrorAction SilentlyContinue
  if ($files.Count -eq 0) { Start-Sleep -Seconds $pollSeconds; continue }

  $statusLines = @()
  $readyFlags = @()
  foreach ($f in $files) {
    $cnt = GetEpisodeEndCount -file $f
    $statusLines += "${f}: episode_end=$cnt"
    $readyFlags += ($cnt -ge $episodesTarget)
  }

  Write-Host ("Status: " + ($statusLines -join ", "))
  $allReady = ($readyFlags -notcontains $false) -and ($files.Count -ge 4)
  if ($allReady) { break }
  Start-Sleep -Seconds $pollSeconds
}

Write-Host "All sweeps reached episode_end >= $episodesTarget for $($files.Count) files. Running summarizer..."

python $summarizer --inputs $pattern --out $csvOut --json $jsonOut

if ($LASTEXITCODE -eq 0) {
  Write-Host "Summary written to $csvOut and $jsonOut"
} else {
  Write-Warning "Summarizer exited with code $LASTEXITCODE"
}