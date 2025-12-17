param(
  [int]$Steps = 5000,
  [int]$ReplayInterval = 100,
  [int]$ReplayTopK = 10,
  [string]$DBPath = "experiments\ADS1_long.db",
  [string]$OutDir = "exports\ADS1_long",
  [string]$PCAOutDir = "",
  [string]$TeacherEmbedPath = "",
  [string]$TripletsRoot = "",
  [string]$MirrorMode = "vision",
  [double]$RewardScale = 1.0,
  [string]$ExePath = ".\neuroforge.exe"
)

if (!(Test-Path (Split-Path -Leaf $ExePath))) {
  if (!(Test-Path $ExePath)) { throw "neuroforge.exe not found at '$ExePath'" }
}

New-Item -ItemType Directory -Force -Path (Split-Path $DBPath) | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if ($PCAOutDir -ne "") { New-Item -ItemType Directory -Force -Path $PCAOutDir | Out-Null }

$argsList = @(
  "--phase-a=on",
  "--mimicry=on",
  "--mimicry-internal=off",
  "--substrate-mode=native",
  "--reward-scale=$RewardScale",
  "--steps=$Steps",
  "--memory-db=$DBPath",
  "--phase-a-replay-interval=$ReplayInterval",
  "--phase-a-replay-top-k=$ReplayTopK"
)

if ($TripletsRoot -ne "") {
  $argsList += @("--dataset-mode=triplets", "--dataset-triplets=$TripletsRoot")
} else {
  if ($TeacherEmbedPath -ne "") { $argsList += @("--teacher-embed=$TeacherEmbedPath") }
  if ($MirrorMode -ne "") { $argsList += @("--mirror-mode=$MirrorMode") }
}

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $ExePath
$psi.Arguments = ($argsList -join " ")
$psi.WorkingDirectory = (Get-Location).Path
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$p = New-Object System.Diagnostics.Process
$p.StartInfo = $psi
$p.Start() | Out-Null
$stdout = $p.StandardOutput.ReadToEnd()
$stderr = $p.StandardError.ReadToEnd()
$p.WaitForExit()
if ($p.ExitCode -ne 0) { Write-Error $stderr; throw "neuroforge.exe exited with code $($p.ExitCode)" }
Write-Host $stdout

python "tools\export_embeddings_rewards.py" --db $DBPath --out_dir $OutDir
if ($LASTEXITCODE -ne 0) { throw "export_embeddings_rewards.py failed" }

if ($PCAOutDir -ne "") {
  python "tools\export_embeddings_rewards.py" --db $DBPath --out_dir $OutDir --pca_out $PCAOutDir
  if ($LASTEXITCODE -ne 0) { Write-Warning "PCA plotting failed" }
}

if (Test-Path "tools\eval_triplet_grounding.py") {
  if ($TripletsRoot -ne "") {
    $cmdline = "$ExePath " + ($argsList -join " ")
    $plotsDir = Join-Path $OutDir "Plots"
    New-Item -ItemType Directory -Force -Path $plotsDir | Out-Null
    python "tools\eval_triplet_grounding.py" --db $DBPath --meta "$OutDir\ADS1_long_meta.json" --dataset $TripletsRoot --cmd "$cmdline" --confusion_out "$OutDir\teacher_confusion.csv" --plots_out "$plotsDir"
  }
}

Write-Host "ADS-1 long-run complete. Outputs in '$OutDir'"
