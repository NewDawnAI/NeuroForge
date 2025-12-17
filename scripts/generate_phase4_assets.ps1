# Generates Phase-4 SVG plots and CLI results table for the paper assets.
# - STDP dt curve (from LearningSystem::calculateSTDPDelta parameters)
# - Eligibility decay trajectory (discrete, no events)
# - Hebbian weight-change histogram (from live+final synapse snapshots in a single run)
# - CLI results table extracted from CTest LastTest.log

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Resolve repository root from this script's location
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Split-Path -Parent $scriptDir
$docsDir = Join-Path $rootDir 'docs\papers\phase1-4'
$figsDir = Join-Path $docsDir 'figs'
$sectionsDir = Join-Path $docsDir 'sections'

if (!(Test-Path $figsDir)) { New-Item -ItemType Directory -Path $figsDir | Out-Null }

function Find-NeuroforgeExe {
  $candidates = @(
    'Debug\neuroforge.exe',
    'build-vcpkg-rel\Debug\neuroforge.exe',
    'build\Debug\neuroforge.exe',
    'build\neuroforge.exe',
    'build-vs\Debug\neuroforge.exe',
    'Release\neuroforge.exe',
    'build-vcpkg-rel\Release\neuroforge.exe',
    'build\Release\neuroforge.exe',
    'build-vs\Release\neuroforge.exe',
    'neuroforge.exe'
  )
  foreach ($rel in $candidates) {
    $p = Join-Path $rootDir $rel
    if (Test-Path $p) { return (Resolve-Path $p).Path }
  }
  return $null
}

function New-STDPPlotSvg {
  param(
    [string]$OutPath,
    [int]$Width = 800,
    [int]$Height = 400
  )
  # STDP parameters from LearningSystem::calculateSTDPDelta
  $tauPlus = 20.0
  $tauMinus = 20.0
  $Aplus = 1.0
  $Aminus = -0.5
  $dtMin = -50
  $dtMax = 50
  $step = 1

  $pts = @()
  for ($dt = $dtMin; $dt -le $dtMax; $dt += $step) {
    if ($dt -gt 0) { $y = $Aplus * [math]::Exp(-$dt / $tauPlus) }
    elseif ($dt -lt 0) { $y = $Aminus * [math]::Exp($dt / $tauMinus) }
    else { $y = 0.0 }
    $pts += [pscustomobject]@{ x = [double]$dt; y = [double]$y }
  }

  # Determine scales
  $padL = 50; $padR = 20; $padT = 20; $padB = 40
  $plotW = $Width - $padL - $padR
  $plotH = $Height - $padT - $padB
  $xscale = $plotW / ($dtMax - $dtMin)
  # y range roughly [-0.5, 1.0]
  $yMin = -0.55; $yMax = 1.05
  $yscale = $plotH / ($yMax - $yMin)

  $poly = @()
  foreach ($p in $pts) {
    $sx = [int]($padL + ($p.x - $dtMin) * $xscale)
    $sy = [int]($padT + ($yMax - $p.y) * $yscale)
    $poly += "$sx,$sy"
  }

  $xticks = @(-50,-25,0,25,50)
  $yticks = @(-0.5,0.0,0.5,1.0)

  $svg = @()
  $svg += "<svg xmlns='http://www.w3.org/2000/svg' width='$Width' height='$Height' viewBox='0 0 $Width $Height'>"
  $svg += "<rect x='0' y='0' width='$Width' height='$Height' fill='white'/>"
  # Axes
  $x0 = $padL; $y0 = $padT + $plotH
  $x1 = $padL + $plotW; $y1 = $padT
  $svg += "<line x1='$x0' y1='$y0' x2='$x1' y2='$y0' stroke='black' stroke-width='1'/>" # x-axis
  $svg += "<line x1='$padL' y1='$y0' x2='$padL' y2='$padT' stroke='black' stroke-width='1'/>" # y-axis

  # Ticks and labels
  foreach ($t in $xticks) {
    $tx = [int]($padL + ($t - $dtMin) * $xscale)
    $svg += "<line x1='$tx' y1='$y0' x2='$tx' y2='$(($y0)+5)' stroke='black'/>"
    $svg += "<text x='$tx' y='$(($y0)+20)' font-size='12' text-anchor='middle'>$t</text>"
  }
  foreach ($t in $yticks) {
    $ty = [int]($padT + ($yMax - $t) * $yscale)
    $svg += "<line x1='$(($padL)-5)' y1='$ty' x2='$padL' y2='$ty' stroke='black'/>"
    $svg += "<text x='$(($padL)-10)' y='$ty' font-size='12' text-anchor='end' dominant-baseline='middle'>$t</text>"
  }

  # Polyline for STDP curve
  $svg += "<polyline fill='none' stroke='#1f77b4' stroke-width='2' points='$(($poly -join ' '))'/>"
  $svg += "<text x='$(($padL + $plotW/2))' y='15' font-size='14' text-anchor='middle'>STDP $([char]0x0394)w vs $([char]0x0394)t</text>"
  $svg += "<text x='$(($padL + $plotW/2))' y='$(($Height-5))' font-size='12' text-anchor='middle'>$([char]0x0394)t (ms)</text>"
  $svg += "<text transform='translate(15,$(($padT + $plotH/2))) rotate(-90)' font-size='12' text-anchor='middle'>$([char]0x0394)w (a.u.)</text>"
  $svg += "</svg>"

  Set-Content -LiteralPath $OutPath -Value ($svg -join "`n") -Encoding UTF8
}

function New-EligibilityDecaySvg {
  param(
    [string]$OutPath,
    [double]$Lambda = 0.8,
    [int]$Steps = 15,
    [int]$Width = 800,
    [int]$Height = 400
  )
  $vals = @()
  $e = 1.0
  for ($t=0; $t -lt $Steps; $t++) { $vals += [pscustomobject]@{ t=$t; e=$e }; $e = $Lambda * $e }

  $padL = 50; $padR = 20; $padT = 20; $padB = 40
  $plotW = $Width - $padL - $padR
  $plotH = $Height - $padT - $padB
  $xscale = $plotW / [math]::Max(1, ($Steps - 1))
  $yMin = 0.0; $yMax = 1.0
  $yscale = $plotH / ($yMax - $yMin)

  $poly = @()
  foreach ($p in $vals) {
    $sx = [int]($padL + $p.t * $xscale)
    $sy = [int]($padT + ($yMax - $p.e) * $yscale)
    $poly += "$sx,$sy"
  }

  $svg = @()
  $svg += "<svg xmlns='http://www.w3.org/2000/svg' width='$Width' height='$Height' viewBox='0 0 $Width $Height'>"
  $svg += "<rect x='0' y='0' width='$Width' height='$Height' fill='white'/>"
  $x0 = $padL; $y0 = $padT + $plotH
  $x1 = $padL + $plotW
  $svg += "<line x1='$x0' y1='$y0' x2='$x1' y2='$y0' stroke='black' stroke-width='1'/>" # x-axis
  $svg += "<line x1='$padL' y1='$y0' x2='$padL' y2='$padT' stroke='black' stroke-width='1'/>" # y-axis

  # Stems and points
  for ($i=0; $i -lt $vals.Count; $i++) {
    $p = $vals[$i]
    $sx = [int]($padL + $p.t * $xscale)
    $sy = [int]($padT + ($yMax - $p.e) * $yscale)
    $svg += "<circle cx='$sx' cy='$sy' r='3' fill='#d62728'/>"
    if ($i -gt 0) {
      $p0 = $vals[$i-1]
      $sx0 = [int]($padL + $p0.t * $xscale)
      $sy0 = [int]($padT + ($yMax - $p0.e) * $yscale)
      $svg += "<line x1='$sx0' y1='$sy0' x2='$sx' y2='$sy' stroke='#d62728' stroke-width='2'/>"
    }
  }

  $svg += "<text x='$(($padL + $plotW/2))' y='15' font-size='14' text-anchor='middle'>Eligibility decay ($([char]0x03BB)=$Lambda)</text>"
  $svg += "<text x='$(($padL + $plotW/2))' y='$(($Height-5))' font-size='12' text-anchor='middle'>Step</text>"
  $svg += "<text transform='translate(15,$(($padT + $plotH/2))) rotate(-90)' font-size='12' text-anchor='middle'>E</text>"
  $svg += "</svg>"

  Set-Content -LiteralPath $OutPath -Value ($svg -join "`n") -Encoding UTF8
}

function New-HistSvg {
  param(
    [double[]]$Data,
    [string]$OutPath,
    [int]$Bins = 30,
    [int]$Width = 800,
    [int]$Height = 400
  )
  if ($Data.Count -eq 0) { throw "No data for histogram" }
  $min = ($Data | Measure-Object -Minimum).Minimum
  $max = ($Data | Measure-Object -Maximum).Maximum
  if ($min -eq $max) { $min -= 1e-6; $max += 1e-6 }
  $binW = ($max - $min) / $Bins
  $counts = @(for ($i=0; $i -lt $Bins; $i++) { 0 })
  foreach ($v in $Data) {
    $idx = [int][math]::Floor(($v - $min) / $binW)
    if ($idx -ge $Bins) { $idx = $Bins - 1 }
    if ($idx -lt 0) { $idx = 0 }
    $counts[$idx]++
  }
  $maxCount = ($counts | Measure-Object -Maximum).Maximum

  $padL = 50; $padR = 20; $padT = 20; $padB = 40
  $plotW = $Width - $padL - $padR
  $plotH = $Height - $padT - $padB
  $barW = [math]::Floor($plotW / $Bins)

  $svg = @()
  $svg += "<svg xmlns='http://www.w3.org/2000/svg' width='$Width' height='$Height' viewBox='0 0 $Width $Height'>"
  $svg += "<rect x='0' y='0' width='$Width' height='$Height' fill='white'/>"
  $x0 = $padL; $y0 = $padT + $plotH
  $x1 = $padL + $plotW
  $svg += "<line x1='$x0' y1='$y0' x2='$x1' y2='$y0' stroke='black' stroke-width='1'/>" # x-axis
  $svg += "<line x1='$padL' y1='$y0' x2='$padL' y2='$padT' stroke='black' stroke-width='1'/>" # y-axis

  for ($i=0; $i -lt $Bins; $i++) {
    $x = $padL + $i * $barW
    $h = if ($maxCount -gt 0) { [int]([double]$counts[$i] / [double]$maxCount * $plotH) } else { 0 }
    $y = $y0 - $h
    $svg += "<rect x='$x' y='$y' width='$(($barW - 1))' height='$h' fill='#2ca02c'/>"
  }

  $svg += "<text x='$(($padL + $plotW/2))' y='15' font-size='14' text-anchor='middle'>Hebbian $([char]0x0394)w distribution</text>"
  $svg += "<text x='$(($padL + $plotW/2))' y='$(($Height-5))' font-size='12' text-anchor='middle'>$([char]0x0394)w</text>"
  $svg += "<text transform='translate(15,$(($padT + $plotH/2))) rotate(-90)' font-size='12' text-anchor='middle'>Count</text>"
  $svg += "</svg>"

  Set-Content -LiteralPath $OutPath -Value ($svg -join "`n") -Encoding UTF8
}

function Get-CSVEdges {
  param([string]$Path)
  $edges = @{ }
  foreach ($line in Get-Content -LiteralPath $Path) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $parts = $line.Trim() -split ','
    if ($parts.Count -lt 3) { continue }
    try {
      $pre = [uint64]$parts[0]
      $post = [uint64]$parts[1]
      $w = [double]$parts[2]
    } catch {
      # likely a header line or malformed row; skip
      continue
    }
    $key = "${pre}:${post}"
    $edges[$key] = $w
  }
  return $edges
}

function Generate-HebbianHistogram {
  param([string]$ExePath)
  $tmpDir = Join-Path $rootDir 'scripts\_tmp_phase4'
  if (!(Test-Path $tmpDir)) { New-Item -ItemType Directory -Path $tmpDir | Out-Null }
  $live = Join-Path $tmpDir 'live.csv'
  $final = Join-Path $tmpDir 'final.csv'

  if (Test-Path $live) { Remove-Item -Force $live }
  if (Test-Path $final) { Remove-Item -Force $final }

  $args = @(
    '--enable-learning', '--steps=400', '--step-ms=1', '--vision-demo=off',
    "--snapshot-live=$live", '--snapshot-interval=25',
    "--snapshot-csv=$final"
  )

  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $ExePath
  $psi.Arguments = ($args -join ' ')
  $psi.WorkingDirectory = $rootDir
  $psi.UseShellExecute = $false
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true
  $proc = New-Object System.Diagnostics.Process
  $proc.StartInfo = $psi
  [void]$proc.Start()

  # Wait for initial live snapshot to appear and have data
  $initial = Join-Path $tmpDir 'initial.csv'
  try { if (Test-Path $initial) { Remove-Item -Force $initial } } catch {}

  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  while ($sw.ElapsedMilliseconds -lt 10000) {
    if (Test-Path $live) {
      $lines = Get-Content -LiteralPath $live -ErrorAction SilentlyContinue
      if ($lines.Count -gt 10) {
        Copy-Item -LiteralPath $live -Destination $initial -Force
        break
      }
    }
    Start-Sleep -Milliseconds 100
  }

  $proc.WaitForExit()
  if ($proc.ExitCode -ne 0) {
    Write-Warning "neuroforge exited with code $($proc.ExitCode); skipping histogram generation"
    return
  }
  if (!(Test-Path $final)) { Write-Warning "Final snapshot CSV not found; skipping histogram"; return }
  if (!(Test-Path $initial)) { Write-Warning "Initial snapshot CSV not captured; skipping histogram"; return }

  $initEdges = Get-CSVEdges -Path $initial
  $finalEdges = Get-CSVEdges -Path $final

  $deltas = New-Object System.Collections.Generic.List[double]
  foreach ($kv in $finalEdges.GetEnumerator()) {
    $key = $kv.Key
    $wf = $kv.Value
    if ($initEdges.ContainsKey($key)) {
      $wi = $initEdges[$key]
      $deltas.Add($wf - $wi) | Out-Null
    }
  }

  if ($deltas.Count -lt 5) { Write-Warning "Not enough matched edges to build histogram"; return }

  $outSvg = Join-Path $figsDir 'hebbian_delta_hist.svg'
  New-HistSvg -Data $deltas.ToArray() -OutPath $outSvg
}

function Generate-CLITable {
  # Prefer known path from previous runs
  $logCandidates = @(
    (Join-Path $rootDir 'build-vcpkg-rel\Testing\Temporary\LastTest.log'),
    (Join-Path $rootDir 'build\Testing\Temporary\LastTest.log'),
    (Join-Path $rootDir 'Testing\Temporary\LastTest.log')
  )
  $logPath = $null
  foreach ($p in $logCandidates) { if (Test-Path $p) { $logPath = $p; break } }
  if (-not $logPath) { Write-Warning 'LastTest.log not found; skipping CLI table generation'; return }

  $content = Get-Content -LiteralPath $logPath
  $shortOK = ($content | Select-String -SimpleMatch 'PASS: CLI accepted valid Phase-4 short flags') -ne $null
  $invalidRej = ($content | Select-String -SimpleMatch 'PASS: CLI rejected --lambda=1.5 with exit code 2') -ne $null
  $unsafeOK = ($content | Select-String -SimpleMatch 'PASS: CLI accepted invalid Phase-4 values when --phase4-unsafe is set') -ne $null

  if ($shortOK) { $row1 = 'Valid short flags & Accepted (exit code 0) \\' } else { $row1 = 'Valid short flags & See log \\' }
  if ($invalidRej) { $row2 = 'Invalid values, unsafe off & Rejected (exit code 2) \\' } else { $row2 = 'Invalid values, unsafe off & See log \\' }
  if ($unsafeOK) { $row3 = 'Invalid values, unsafe on & Accepted (exit code 0) \\' } else { $row3 = 'Invalid values, unsafe on & See log \\' }

  $tex = @()
  $tex += "\\begin{table}[t]"
  $tex += "  \\centering"
  $tex += "  \\caption{CLI scenarios and outcomes for Phase-4 flags.}"
  $tex += "  \\label{tab:cli}"
  $tex += "  \\begin{tabular}{ll}"
  $tex += "    \\toprule"
  $tex += "    Scenario & Outcome \\\\" 
  $tex += "    \\midrule"
  $tex += "    $row1"
  $tex += "    $row2"
  $tex += "    $row3"
  $tex += "    \\bottomrule"
  $tex += "  \\end{tabular}"
  $tex += "\\end{table}"

  $outPath = Join-Path $sectionsDir 'cli_results_table.tex'
  Set-Content -LiteralPath $outPath -Value ($tex -join "`n") -Encoding UTF8
}

# Generate STDP and Eligibility plots (deterministic, no external deps)
New-STDPPlotSvg -OutPath (Join-Path $figsDir 'stdp_dt_curve.svg')
New-EligibilityDecaySvg -OutPath (Join-Path $figsDir 'eligibility_decay.svg')

# Generate Hebbian histogram if executable is available
$exe = Find-NeuroforgeExe
if ($exe) {
  Generate-HebbianHistogram -ExePath $exe
} else {
  Write-Warning 'neuroforge.exe not found; skipping Hebbian histogram generation'
}

# Build CLI results LaTeX table from CTest logs (if present)
Generate-CLITable

Write-Host "Phase-4 assets generation completed. Outputs in: $figsDir and $sectionsDir"