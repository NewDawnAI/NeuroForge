param(
  [string]$Exe = ".\\build\\neuroforge.exe",
  [string]$Seeds = "7,13,21,42,99",
  [int]$Steps = 5000,
  [switch]$Live,
  [string]$LiveMode = "match",
  [int]$MatchWindow = 500,
  [double]$MinMatch = 0.80,
  [double]$MinAdvantage = 17.0,
  [double]$MinPredDistAdvantage = 0.0,
  [double]$MaxCollisionProb = 0.05,
  [double]$MinDistImprove = 0.05,
  [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($OutDir)) {
  $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
  $OutDir = ".\\Artifacts\\UnifiedBoundedSweeps\\$stamp"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$seedList = @()
foreach ($tok in ($Seeds -split "[,\\s]+")) {
  if ([string]::IsNullOrWhiteSpace($tok)) { continue }
  $seedList += [int]$tok
}
if ($seedList.Count -eq 0) {
  throw 'No seeds provided. Use -Seeds "7,13,21".'
}

$summaryRows = @()

foreach ($seed in $seedList) {
  $tag = "seed_$seed"
  $logPath = Join-Path $OutDir "unified_bounded_log_$tag.csv"
  $sumPath = Join-Path $OutDir "unified_bounded_summary_$tag.csv"

  $args = @(
    "--unified-bounded",
    "--unified-bounded-steps=$Steps",
    "--unified-bounded-seed=$seed",
    "--unified-bounded-log=$logPath",
    "--unified-bounded-summary=$sumPath"
  )

  if ($Live) {
    $args += @(
      "--unified-bounded-live=on",
      "--unified-bounded-live-mode=$LiveMode",
      "--unified-bounded-live-match-window=$MatchWindow",
      "--unified-bounded-live-min-match=$MinMatch",
      "--unified-bounded-live-min-advantage=$MinAdvantage",
      "--unified-bounded-live-min-pred-dist-advantage=$MinPredDistAdvantage",
      "--unified-bounded-live-max-collision-prob=$MaxCollisionProb",
      "--unified-bounded-live-min-dist-improve=$MinDistImprove"
    )
  }

  Write-Host "[Sweep] seed=$seed live=$($Live.IsPresent) steps=$Steps"
  & $Exe @args | Out-Host
  if ($LASTEXITCODE -ne 0) {
    throw "Run failed for seed=$seed (exit=$LASTEXITCODE)"
  }

  $row = Import-Csv $sumPath | Select-Object -First 1
  $obj = [ordered]@{
    seed = $seed
    live = [int]$Live.IsPresent
    live_mode = if ($Live) { $LiveMode } else { "" }
    steps = $Steps
    match_window = if ($Live) { $MatchWindow } else { 0 }
    min_match = if ($Live) { $MinMatch } else { 0.0 }
    min_advantage = if ($Live) { $MinAdvantage } else { 0.0 }
    min_pred_dist_advantage = if ($Live) { $MinPredDistAdvantage } else { 0.0 }
    max_collision_prob = if ($Live) { $MaxCollisionProb } else { 0.0 }
    min_dist_improve = if ($Live) { $MinDistImprove } else { 0.0 }
  }
  foreach ($p in $row.PSObject.Properties.Name) {
    $obj[$p] = $row.$p
  }
  $summaryRows += New-Object psobject -Property $obj
}

$outCsv = Join-Path $OutDir "sweep_summary.csv"
$summaryRows | Export-Csv -NoTypeInformation -Path $outCsv

Write-Host ""
Write-Host "[Sweep] Wrote $outCsv"
Write-Host "[Sweep] Mean success_rate: " -NoNewline
("{0:P3}" -f (($summaryRows | Measure-Object -Property success_rate -Average).Average)) | Write-Host
Write-Host "[Sweep] Mean wm_drive_step_rate: " -NoNewline
("{0:P3}" -f (($summaryRows | Measure-Object -Property wm_drive_step_rate -Average).Average)) | Write-Host
