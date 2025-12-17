Param(
  [string]$OutDir = "Demo\Phase5_Demo_Bundle",
  [switch]$Zip
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$OutPath = Join-Path $Root $OutDir
$DocsPath = Join-Path $OutPath 'docs'
$ChartsPath = Join-Path $OutPath 'charts'
$DataPath = Join-Path $OutPath 'data'
$ScriptsPath = Join-Path $OutPath 'scripts'

# Create directories
$null = New-Item -ItemType Directory -Force -Path $OutPath, $DocsPath, $ChartsPath, $DataPath, $ScriptsPath

# Source artifacts
$onePagerHtml = Join-Path $Root 'Lab_Log\Phase5_External_OnePager.html'
$onePagerPdf  = Join-Path $Root 'Lab_Log\Phase5_External_OnePager.pdf'
$configJson   = Join-Path $Root 'Lab_Log\Phase5_Config_Freeze.json'
$chartIndex   = Join-Path $Root 'Lab_Log\Phase5_Chart_Index.txt'

# Copy docs
foreach ($src in @($onePagerHtml, $onePagerPdf, $configJson, $chartIndex)) {
  if (Test-Path $src) { Copy-Item $src -Destination $DocsPath -Force }
}

# Collect charts (SVG) and data (CSV)
$buildDir = Join-Path $Root 'build'
$releaseDir = Join-Path $Root 'build\Release'

if (Test-Path $buildDir) {
  Get-ChildItem $buildDir -File -Filter 'phase5_actions_*.svg' | Copy-Item -Destination $ChartsPath -Force -ErrorAction SilentlyContinue
  Get-ChildItem $buildDir -File -Filter 'phase5_actions_*.csv' | Copy-Item -Destination $DataPath   -Force -ErrorAction SilentlyContinue
}
if (Test-Path $releaseDir) {
  Get-ChildItem $releaseDir -File -Filter 'phase5*.csv' | Copy-Item -Destination $DataPath -Force -ErrorAction SilentlyContinue
}

# Include export script for reproducible PDF generation
$exportScript = Join-Path $Root 'tools\export_onepager_pdf.ps1'
if (Test-Path $exportScript) { Copy-Item $exportScript -Destination $ScriptsPath -Force }

# Create a tiny demo launcher that opens the one-pager and a few representative charts
$demoLauncher = @'
Param([switch]$NoCharts)
$Base    = Split-Path -Parent $PSScriptRoot
$Docs    = Join-Path $Base 'docs'
$Charts  = Join-Path $Base 'charts'
$OnePager = Join-Path $Docs 'Phase5_External_OnePager.pdf'
if (Test-Path $OnePager) { Start-Process $OnePager }
if (-not $NoCharts) {
  $candidates = @(
    'phase5_actions_modA_episode_rewards.svg',
    'phase5_actions_long_episode_rewards.svg',
    'phase5_actions_A2_attn_alignment.svg'
  )
  foreach ($f in $candidates) {
    $p = Join-Path $Charts $f
    if (Test-Path $p) { Start-Process $p }
  }
}
Write-Host 'Phase 5 demo opened (one-pager + representative charts).'
'@

$demoLauncherPath = Join-Path $ScriptsPath 'run_demo.ps1'
$demoLauncher | Set-Content -Path $demoLauncherPath -Encoding UTF8

# Optionally zip the bundle
if ($Zip) {
  $zipPath = Join-Path (Split-Path $OutPath -Parent) 'Phase5_Demo_Bundle.zip'
  if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
  Compress-Archive -Path (Join-Path $OutPath '*') -DestinationPath $zipPath -Force
  Write-Host "Bundle zipped => $zipPath"
}

Write-Host "Bundle assembled => $OutPath"
Write-Host "Docs:    $DocsPath"
Write-Host "Charts:  $ChartsPath"
Write-Host "Data:    $DataPath"
Write-Host "Scripts: $ScriptsPath"