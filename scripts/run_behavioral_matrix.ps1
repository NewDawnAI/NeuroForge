# PowerShell Sweep Script: Phase 15 Behavioral Matrix
# Automates modulation-off, ±rate sweeps, long-run baseline, hysteresis generation,
# and regenerates analysis artifacts.

param(
  [double[]] $Thresholds = @(0.2, 0.3, 0.4),
  [int[]] $Seeds = @(1, 2, 3),
  [int] $Window = 50,
  [int] $StepsShort = 5000,
  [int] $StepsLong = 10000,
  [double] $HebbianBase = 0.011,
  [double] $StdpBase = 0.004
)

function Run-Phase15 {
  param(
    [double] $thr,
    [int] $seed,
    [string] $modulation, # 'on' or 'off'
    [int] $steps,
    [double] $hebbian,
    [double] $stdp,
    [string] $tag
  )
  $db = "web/phase15_thr$($thr)_seed$($seed)_$($tag).db"
  & .\build\neuroforge.exe `
    --phase6=on --phase7=on --phase9=on --phase9-modulation=$modulation `
    --phase15=on --phase15-window=$Window --phase15-risk-threshold=$thr `
    --enable-learning --hebbian-rate=$hebbian --stdp-rate=$stdp `
    --steps=$steps --seed=$seed --log-json=on --viewer=off --memory-db=$db
  python scripts\dump_metacognition.py --db $db --out "web/ethics_$($thr)_seed$($seed)_$($tag).json"
}

Write-Host "[Sweep] Modulation OFF batch (short runs)" -ForegroundColor Cyan
foreach ($thr in $Thresholds) {
  foreach ($seed in $Seeds) {
    Run-Phase15 -thr $thr -seed $seed -modulation "off" -steps $StepsShort -hebbian $HebbianBase -stdp $StdpBase -tag "modoff"
  }
}

Write-Host "[Sweep] Learning rate -10% batch (short runs)" -ForegroundColor Cyan
$hebbianLow = [math]::Round($HebbianBase * 0.90, 6)
$stdpLow = [math]::Round($StdpBase * 0.90, 6)
foreach ($thr in $Thresholds) {
  foreach ($seed in $Seeds) {
    Run-Phase15 -thr $thr -seed $seed -modulation "on" -steps $StepsShort -hebbian $hebbianLow -stdp $stdpLow -tag "rateLow"
  }
}

Write-Host "[Sweep] Learning rate +10% batch (short runs)" -ForegroundColor Cyan
$hebbianHigh = [math]::Round($HebbianBase * 1.10, 6)
$stdpHigh = [math]::Round($StdpBase * 1.10, 6)
foreach ($thr in $Thresholds) {
  foreach ($seed in $Seeds) {
    Run-Phase15 -thr $thr -seed $seed -modulation "on" -steps $StepsShort -hebbian $hebbianHigh -stdp $stdpHigh -tag "rateHigh"
  }
}

Write-Host "[Sweep] Long-run baseline at 0.3 (10k steps)" -ForegroundColor Cyan
foreach ($seed in $Seeds) {
  Run-Phase15 -thr 0.3 -seed $seed -modulation "on" -steps $StepsLong -hebbian $HebbianBase -stdp $StdpBase -tag "long"
}

Write-Host "[Sweep] Generate hysteresis trajectory (0.2 → 0.4 → 0.3)" -ForegroundColor Cyan
python scripts\generate_hysteresis.py --out "web/hysteresis_0.2_0.4_0.3.json" --segments "0.2:1500,0.4:1500,0.3:1500" --window $Window --seed 42

Write-Host "[Sweep] Regenerating analysis artifacts" -ForegroundColor Cyan
python scripts\analyze_ethics.py

Write-Host "[Sweep] Completed behavioral matrix." -ForegroundColor Green

