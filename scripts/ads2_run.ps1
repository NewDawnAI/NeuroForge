param(
    # Name for this run; controls export/db directories
    [string]$RunName = "ADS2_run01",
    # Triplets dataset root directory
    [string]$Dataset = "data\triplets",
    # Max number of triplets to use
    [int]$Limit = 800,
    # Training step count (overrides hardcoded 6000)
    [int]$Steps = 10000
)

$bin = "build\neuroforge.exe"
if (!(Test-Path $bin)) { Write-Error "Binary not found: $bin"; exit 1 }

$ds = $Dataset
if (!(Test-Path $ds)) { Write-Error "Dataset not found: $ds"; exit 1 }

$expdir = "experiments"
$exportDir = "exports\$RunName"
$plotsDir = "$exportDir\Plots"
$pcaDir = "$exportDir\PCA"
New-Item -ItemType Directory -Force -Path $expdir, $exportDir, $plotsDir, $pcaDir | Out-Null

$db = "$expdir\$RunName.db"

& $bin `
    --steps=$Steps `
    --dataset-mode=triplets `
    --dataset-triplets=$ds `
    --dataset-limit=$Limit `
    --phase-a=on `
    --phase5-language=on `
    --mimicry-internal=off `
    --memory-db=$db `
    --hippocampal-snapshots=on `
    --reward-scale=3.0 `
    --student-learning-rate=0.018 `
    --phase-a-ema=on `
    --phase-a-ema-min=0.01 `
    --phase-a-ema-max=0.08 `
    --phase-a-similarity-threshold=0.10 `
    --phase-a-novelty-threshold=0.10 `
    --negative-sampling-k=3 `
    --negative-weight=0.05 `
    --phase-a-replay-interval=200 `
    --phase-a-replay-top-k=5 `
    --phase-a-replay-boost=1.1 `
    --phase-a-replay-lr-scale=1.05 `
    --phase-a-replay-include-hard-negatives=on `
    --phase-a-replay-hard-k=3 `
    --phase-a-replay-repulsion-weight=0.10 `
    --enable-selfnode=on `
    --enable-pfc=on `
    --substrate-mode=native `
    --telemetry-extended=on `
    --log-json=off

python tools\export_embeddings_rewards.py `
    --db $db `
    --out_dir $exportDir `
    --pca_out $pcaDir

python tools\eval_triplet_grounding.py `
    --db $db `
    --dataset $ds `
    --cmd "ADS2_triplets_eval" `
    --limit $Limit `
    --meta $exportDir\run_meta.json `
    --plots_out $plotsDir `
    --confusion_out $exportDir\teacher_confusion.csv
