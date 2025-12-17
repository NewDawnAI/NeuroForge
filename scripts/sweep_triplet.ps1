param(
    [string]$DatasetRoot = "c:\Users\ashis\Desktop\NeuroForge\flickr30k_triplets",
    [string]$BaseDir = "experiments\m1_triplet_grounding_sweep",
    [int]$Steps = 2000,
    [int]$StepMs = 10,
    [int]$Limit = 200,
    [int]$MemdbInterval = 250
)

New-Item -ItemType Directory -Path $BaseDir -Force | Out-Null

$LRs = @(0.0005, 0.001, 0.003, 0.005, 0.01, 0.02, 0.05)
$RSs = @(0.5, 1.0, 2.0, 4.0, 8.0)

$csvPath = Join-Path $BaseDir "sweep_results_summary.csv"
if (!(Test-Path $csvPath)) {
"reward_scale,student_lr,recall@1,recall@5,grounding_accuracy,token_activation_stability,inter_sample_generalization,similarity_mean,similarity_count,run_dir" | Out-File -Encoding utf8 $csvPath
}

foreach ($lr in $LRs) {
  foreach ($rs in $RSs) {
      $runDir = Join-Path $BaseDir ("lr_" + $lr.ToString().Replace('.', '_') + "__rs_" + $rs.ToString().Replace('.', '_'))
      New-Item -ItemType Directory -Path $runDir -Force | Out-Null
      $db = Join-Path $runDir "mem.db"
      $meta = Join-Path $runDir "run_meta.json"
      $plots = Join-Path $runDir "plots"
      $conf = Join-Path $runDir "confusion.csv"
      $cmd = "`".\build\neuroforge.exe`" --steps=$Steps --step-ms=$StepMs --dataset-triplets=`"$DatasetRoot`" --dataset-mode=triplets --dataset-limit=$Limit --memory-db=`"$db`" --memdb-interval=$MemdbInterval --phase-a=on --enable-learning --mimicry=on --mirror-mode=vision --telemetry-extended=on --vision-grid=16 --student-learning-rate=$lr --reward-scale=$rs --phase-a-similarity-threshold=0.05 --phase-a-novelty-threshold=0.0"
      & .\build\neuroforge.exe `
        --steps=$Steps `
        --step-ms=$StepMs `
        --dataset-triplets="$DatasetRoot" `
        --dataset-mode=triplets `
        --dataset-limit=$Limit `
        --memory-db="$db" `
        --memdb-interval=$MemdbInterval `
        --phase-a=on `
        --enable-learning `
        --mimicry=on `
        --mirror-mode=vision `
        --telemetry-extended=on `
        --vision-grid=16 `
        --student-learning-rate=$lr `
        --reward-scale=$rs `
        --phase-a-similarity-threshold=0.05 `
        --phase-a-novelty-threshold=0.0
      $py = python tools/eval_triplet_grounding.py `
        --db $db `
        --meta $meta `
        --dataset $DatasetRoot `
        --cmd $cmd `
        --limit $Limit `
        --window_ms 5000 `
        --confusion_out $conf `
        --plots_out $plots
      try {
        $metrics = Get-Content $meta | ConvertFrom-Json
        $m = $metrics.eval_triplet_grounding
        $line = ("{0},{1},{2},{3},{4},{5},{6},{7},{8},{9}" -f $rs, $lr, $m."recall@1", $m."recall@5", $m.grounding_accuracy, $m.token_activation_stability, $m.inter_sample_generalization, $m.similarity_mean, $m.similarity_count, $runDir)
        Add-Content -Path $csvPath -Value $line
      } catch {}
  }
}
