param(
  [string]$Dir = ".\\Artifacts\\UnifiedBoundedSweeps\\latest_live_diag"
)

$ErrorActionPreference = "Stop"

if (!(Test-Path -Path $Dir)) {
  throw "Directory not found: $Dir"
}

$files = Get-ChildItem -Path $Dir -Filter "unified_bounded_log_*.csv" -File | Sort-Object Name
if ($files.Count -eq 0) {
  throw "No unified_bounded_log_*.csv files found in: $Dir"
}

$rows = @()

foreach ($f in $files) {
  $seed = $null
  if ($f.BaseName -match 'seed_(\d+)$') {
    $seed = [int]$Matches[1]
  }

  $data = Import-Csv $f.FullName | Where-Object { $_.mode -ne "warmup" }
  $total = $data.Count
  if ($total -le 0) { continue }

  $groups = $data | Group-Object gate_block_reason
  foreach ($g in $groups) {
    $rows += [pscustomobject]@{
      seed = $seed
      file = $f.Name
      post_warmup_steps = $total
      gate_block_reason = $g.Name
      count = $g.Count
      frac = [double]$g.Count / [double]$total
    }
  }
}

$outCsv = Join-Path $Dir "gate_block_summary.csv"
$rows | Sort-Object seed, gate_block_reason | Export-Csv -NoTypeInformation -Path $outCsv

Write-Host "[GateReport] Wrote $outCsv"
Write-Host ""
Write-Host "[GateReport] Aggregate (all seeds):"

$agg = $rows | Group-Object gate_block_reason | ForEach-Object {
  $c = ($_.Group | Measure-Object -Property count -Sum).Sum
  [pscustomobject]@{ gate_block_reason = $_.Name; count = [int]$c }
} | Sort-Object count -Descending

$agg | Format-Table -AutoSize
