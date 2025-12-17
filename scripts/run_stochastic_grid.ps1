param(
  [Parameter(Mandatory=$false)] [int[]] $Seeds = @(1,2,3,4,5),
  [Parameter(Mandatory=$false)] [double[]] $NoiseLevels = @(0.00,0.02,0.04,0.06,0.08,0.10,0.12),
  [Parameter(Mandatory=$false)] [double[]] $RateMultipliers = @(0.8,0.9,1.0,1.1,1.2),
  [Parameter(Mandatory=$false)] [int] $Steps = 5000,
  [Parameter(Mandatory=$false)] [double] $Threshold = 0.3,
  [switch] $Resume,
  [switch] $PlanOnly
)

function Run-One {
  param(
    [int] $Seed,
    [double] $RateMult,
    [int] $Steps,
    [double] $Threshold,
    [double[]] $NoiseLevels
  )

  $hebBase = 0.011
  $stdpBase = 0.004
  $hebRate = [math]::Round($hebBase * $RateMult, 6)
  $stdpRate = [math]::Round($stdpBase * $RateMult, 6)

  $memdb = "web/phase16_thr${Threshold}_seed${Seed}_rate${RateMult}.db"
  $ethicsOut = "web/ethics_${Threshold}_seed${Seed}_rate${RateMult}.json"
  $telemetryOut = "web/syslog_stoch_seed${Seed}_rate${RateMult}.json"

  Write-Host "[Grid] Seed=$Seed RateMult=$RateMult Steps=$Steps thr=$Threshold" -ForegroundColor Cyan

  # Start telemetry collector
  $telemetryProc = Start-Process -FilePath "python" -ArgumentList "scripts/collect_sys_stats.py","--interval","2","--out","$telemetryOut" -PassThru -WindowStyle Hidden

  try {
    # Run simulator
    .\build\neuroforge.exe `
      --phase6=on --phase7=on --phase9=on `
      --phase15=on --phase15-window=50 --phase15-risk-threshold=$Threshold `
      --enable-learning --hebbian-rate=$hebRate --stdp-rate=$stdpRate `
      --steps=$Steps --seed=$Seed --log-json=on --viewer=off `
      --memory-db="$memdb"

    # Dump ethics JSON
    python scripts/dump_metacognition.py --db "$memdb" --out "$ethicsOut"
  }
  finally {
    if ($telemetryProc -and $telemetryProc.Id) {
      try { Stop-Process -Id $telemetryProc.Id -Force } catch { }
    }
  }

  # Generate noise-injected variants
  foreach ($sigma in $NoiseLevels) {
    $noiseOut = "$($ethicsOut.TrimEnd('.json'))_noise$($sigma.ToString('0.00')).json"
    if ($Resume -and (Test-Path $noiseOut)) {
      Write-Host "[Skip] Exists: $noiseOut" -ForegroundColor Yellow
      continue
    }
    python scripts/inject_risk_noise.py --in "$ethicsOut" --out "$noiseOut" --std $sigma
  }
}

function Write-BatchPlan {
  param([int[]] $Seeds, [double[]] $RateMultipliers, [double[]] $NoiseLevels, [double] $Threshold)
  $plan = @()
  foreach ($seed in $Seeds) {
    foreach ($rm in $RateMultipliers) {
      $ethicsOut = "web/ethics_${Threshold}_seed${seed}_rate${rm}.json"
      foreach ($sigma in $NoiseLevels) {
        $noiseOut = "$($ethicsOut.TrimEnd('.json'))_noise$($sigma.ToString('0.00')).json"
        $plan += @{ seed=$seed; rate=$rm; noise=$sigma; base="$ethicsOut"; out="$noiseOut" }
      }
    }
  }
  $outDir = "Artifacts/JSON"
  if (!(Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
  $outPath = "$outDir/Phase16c_BatchPlan.json"
  $plan | ConvertTo-Json | Set-Content -Path $outPath -Encoding UTF8
  Write-Host "[Plan] Wrote $outPath with $($plan.Count) jobs" -ForegroundColor Cyan
}

Write-BatchPlan -Seeds $Seeds -RateMultipliers $RateMultipliers -NoiseLevels $NoiseLevels -Threshold $Threshold
if ($PlanOnly) {
  Write-Host "[Grid] Plan-only requested; skipping execution." -ForegroundColor Yellow
} else {
  # Main grid (sequential with resume-safe skipping of noise variants)
  foreach ($seed in $Seeds) {
    foreach ($rm in $RateMultipliers) {
      Run-One -Seed $seed -RateMult $rm -Steps $Steps -Threshold $Threshold -NoiseLevels $NoiseLevels -Resume:$Resume
    }
  }
}

# Build Phase 16 scaling & coupling SVGs and Phase 16b surface (if grid populated)
python scripts/analyze_stochastic_scaling.py
python scripts/analyze_stochastic_surface.py

Write-Host "[Grid] Completed. See Artifacts/SVG/Phase16_StochasticScaling.svg, Phase16_TemporalCoupling.svg, Phase16_StabilitySurface.svg" -ForegroundColor Green
