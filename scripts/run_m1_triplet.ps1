# Reproducible run script for M1 Triplet Grounding (200 triplets, 5k steps)
# Creates experiment folder, runs neuroforge.exe, and updates run_meta.json via evaluator.

param(
    [string]$DatasetRoot = "c:\Users\ashis\Desktop\NeuroForge\flickr30k_triplets",
    [string]$ExperimentDir = "experiments\m1_triplet_grounding",
    [int]$Steps = 5000,
    [int]$StepMs = 10,
    [int]$Limit = 200,
    [int]$WindowMs = 5000,
    [double]$StudentLR = 0.005,
    [double]$RewardScale = 1.0,
    [int]$MimicryRepeats = 5,
    [int]$NegativeK = 5,
    [double]$NegativeWeight = 0.2,
    [double]$SimilarityThreshold = 0.05
)

# Ensure experiment directory exists
New-Item -ItemType Directory -Path $ExperimentDir -Force | Out-Null

# Paths for memory DB and meta
$db = Join-Path $ExperimentDir "mem.db"
$meta = Join-Path $ExperimentDir "run_meta.json"

$exePath = ".\build\neuroforge.exe"
if (-not (Test-Path $exePath)) { $exePath = ".\build-vcpkg-msvc\Release\neuroforge.exe" }

# Compose command line used for the run
$cmd = "`"$exePath`" " +
       "--steps=$Steps --step-ms=$StepMs " +
       "--dataset-triplets=`"$DatasetRoot`" --dataset-mode=triplets --dataset-limit=$Limit " +
       "--memory-db=`"$db`" --memdb-interval=500 " +
       "--phase-a=on --phase5-language=on --enable-learning --mimicry=on --mirror-mode=vision " +
       "--telemetry-extended=on --vision-grid=16 " +
       "--student-learning-rate=$StudentLR --reward-scale=$RewardScale " +
       "--phase-a-mimicry-repeats=$MimicryRepeats --negative-sampling-k=$NegativeK --negative-weight=$NegativeWeight " +
       "--phase-a-similarity-threshold=$SimilarityThreshold --phase-a-novelty-threshold=0.0"

# Execute the run
Write-Host "Running NeuroForge with triplet dataset..." -ForegroundColor Cyan
& $exePath `
    --steps=$Steps `
    --step-ms=$StepMs `
    --dataset-triplets=$DatasetRoot `
    --dataset-mode=triplets `
    --dataset-limit=$Limit `
    --memory-db=$db `
    --memdb-interval=500 `
    --phase-a=on `
    --phase5-language=on `
    --enable-learning `
    --mimicry=on `
    --mirror-mode=vision `
    --telemetry-extended=on `
    --vision-grid=16 `
    --student-learning-rate=$StudentLR `
    --reward-scale=$RewardScale `
    --phase-a-mimicry-repeats=$MimicryRepeats `
    --negative-sampling-k=$NegativeK `
    --negative-weight=$NegativeWeight `
    --phase-a-similarity-threshold=$SimilarityThreshold `
    --phase-a-novelty-threshold=0.0

# Evaluate and update run_meta.json
Write-Host "Evaluating triplet grounding and updating run_meta.json..." -ForegroundColor Cyan
python tools/eval_triplet_grounding.py `
    --db $db `
    --meta $meta `
    --dataset $DatasetRoot `
    --cmd $cmd `
    --limit $Limit `
    --window_ms $WindowMs

# Show summary from meta
if (Test-Path $meta) {
    $j = Get-Content $meta | ConvertFrom-Json
    Write-Host "Recall@1: $($j.eval_triplet_grounding."recall@1")" -ForegroundColor Green
    Write-Host "Recall@5: $($j.eval_triplet_grounding."recall@5")" -ForegroundColor Green
    Write-Host "Grounding Accuracy: $($j.eval_triplet_grounding.grounding_accuracy)" -ForegroundColor Green
}
