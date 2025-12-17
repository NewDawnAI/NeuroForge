param(
  [string]$Pattern = 'phase6_*.jsonl',
  [string]$Out = 'phase6_sweep_summary.csv'
)

function Get-Correlation([double[]]$x, [double[]]$y) {
  if ($null -eq $x -or $null -eq $y) { return 'n/a' }
  $n = [Math]::Min($x.Count, $y.Count)
  if ($n -lt 2) { return 'n/a' }
  $xs = $x[0..($n-1)]
  $ys = $y[0..($n-1)]
  $mx = ($xs | Measure-Object -Average).Average
  $my = ($ys | Measure-Object -Average).Average
  $sum = 0.0; $sx = 0.0; $sy = 0.0
  for ($i=0; $i -lt $n; $i++) {
    $dx = $xs[$i] - $mx
    $dy = $ys[$i] - $my
    $sum += ($dx * $dy)
    $sx += ($dx * $dx)
    $sy += ($dy * $dy)
  }
  if ($sx -eq 0 -or $sy -eq 0) { return 'n/a' }
  return [Math]::Round(($sum / ([Math]::Sqrt($sx) * [Math]::Sqrt($sy))), 6)
}

$files = Get-ChildItem -File -Name $Pattern | Sort-Object
if ($files.Count -eq 0) {
  Write-Host "No files matching $Pattern"; exit 1
}

$rows = @()
foreach ($f in $files) {
  $path = Join-Path (Get-Location) $f
  $lines = Get-Content $path
  $total = $lines.Count
  $ep_start = ($lines | Select-String '"event":"episode_start"').Count
  $ep_end = ($lines | Select-String '"event":"episode_end"').Count
  $gate_lines = $lines | Select-String '"phase":"6".*"event":"gate"'
  $gate = $gate_lines.Count
  $overrides_true = ($gate_lines | Select-String '"override_applied":true').Count
  $override_rate = if ($gate -gt 0) { [Math]::Round(($overrides_true / $gate), 6) } else { 0 }
  $contradictions = 0
  foreach ($m in $gate_lines) {
    if ($m.Line -match '"policy_action":([0-9]+).*"phase6_action":([0-9]+)') {
      $pa = [int]$Matches[1]; $p6a = [int]$Matches[2]
      if ($pa -ne $p6a) { $contradictions++ }
    }
  }
  $contradiction_rate = if ($gate -gt 0) { [Math]::Round(($contradictions / $gate), 6) } else { 0 }

  # Extract scores for correlation
  $policy_scores = @()
  $phase6_scores = @()
  foreach ($m in $gate_lines) {
    if ($m.Line -match '"policy_score":([\-0-9\.Ee]+)') { $null = $policy_scores += [double]$Matches[1] }
    if ($m.Line -match '"phase6_score":([\-0-9\.Ee]+)') { $null = $phase6_scores += [double]$Matches[1] }
  }

  # Rewards
  $reward_lines = $lines | Select-String '"event":"reward"'
  $reward_vals = @()
  foreach ($m in $reward_lines) { if ($m.Line -match '"reward":([\-0-9\.Ee]+)') { $null = $reward_vals += [double]$Matches[1] } }
  $avg_reward = if ($reward_vals.Count -gt 0) { [Math]::Round(($reward_vals | Measure-Object -Average).Average, 6) } else { 'n/a' }

  # Correlations (pair by sequence index; assumes near-step alignment)
  $corr_policy_reward = Get-Correlation $policy_scores $reward_vals
  $corr_phase6_reward = Get-Correlation $phase6_scores $reward_vals

  # Infer margin from filename if present like phase6_mXX.jsonl
  $margin = ''
  if ($f -match 'phase6_m([0-9]{2})') { $margin = [int]$Matches[1] / 100.0 }

  $rows += [PSCustomObject]@{
    file = $f
    margin = $margin
    lines = $total
    episodes_start = $ep_start
    episodes_end = $ep_end
    gates = $gate
    overrides = $overrides_true
    override_rate = $override_rate
    contradiction_rate = $contradiction_rate
    avg_reward = $avg_reward
    corr_policy_reward = $corr_policy_reward
    corr_phase6_reward = $corr_phase6_reward
  }
}

$rows | Export-Csv -Path $Out -NoTypeInformation
Write-Host "Wrote summary: $Out"