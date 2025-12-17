# Phase C Rollups: σ and Lag Correlations

This page is a structured experiment template and lab notebook for Phase C adaptive modulation telemetry. It gives you reproducible one‑liners to run, capture, and analyze survival_mod events, then compute rollups (variance and lag correlations) suitable for dashboards and comparisons.

## Purpose
- Establish fixed vs. audio‑driven hazard baselines.
- Make adaptive scaling visible via `effective_weight` in JSONL.
- Produce clean CSV + JSONL artifacts for downstream analytics.
- Compute volatility `σ(effective_weight)` and lagged correlations.

## Prereqs
- Run commands from the directory containing `neuroforge.exe` (e.g., `build-ninja` or `build`).
- Logs write under `PhaseC_Logs` relative to the working directory.
- Python available to run `scripts/phase_c_compute_rollups.py`.

## Preset Runs

Baseline (fixed hazard density):

```
./neuroforge.exe --phase-c=on --phase-c-mode=binding \
  --phase-c-survival-bias=on \
  --hazard-density=0.85 \
  --steps=300 --step-ms=50 \
  --log-json=PhaseC_Logs\telemetry_baseline.jsonl \
  --log-json-events=C:survival_mod
```

Audio‑driven hazard with adaptive scaling (visible `effective_weight` changes):

```
./neuroforge.exe --phase-c=on --phase-c-mode=binding \
  --phase-c-survival-bias=on \
  --audio-demo=on \
  --steps=300 --step-ms=50 \
  --phase-c-hazard-alpha=0.8 \
  --phase-c-hazard-beta=0.4 \
  --log-json=PhaseC_Logs\telemetry_alpha_beta.jsonl \
  --log-json-events=C:survival_mod
```

Tips:
- Add `--log-json-sample=5` to reduce event volume.
- Switch task mode with `--phase-c-mode=sequence` if desired.

## Quick Inspect

PowerShell snippet to glance at survival_mod events:

```
Get-Content PhaseC_Logs\telemetry_alpha_beta.jsonl |
  Select-String '"event":"survival_mod"' |
  Select-Object -First 10
```

## Outputs
- `telemetry_*.jsonl` — raw JSONL stream of survival_mod events.
- `assemblies.csv`, `bindings.csv`, `timeline.csv`, `working_memory.csv` — structural snapshots per step.

## Analysis Chain

Compute rollups for baseline vs. audio runs (adjust paths as needed):

```
python scripts/phase_c_compute_rollups.py \
  --events PhaseC_Logs\telemetry_baseline.jsonl \
  --max-lag 200 \
  --out-csv PhaseC_Logs\rollups_baseline.csv

python scripts/phase_c_compute_rollups.py \
  --events PhaseC_Logs\telemetry_alpha_beta.jsonl \
  --max-lag 200 \
  --out-csv PhaseC_Logs\rollups_audio.csv
```

Expected metrics include:
- `sigma_effective_weight` — volatility of applied modulation.
- `corr_effective_reward`, `lag_peak` — coupling signatures to environment.
- Hazard statistics — control envelope under fixed vs. audio.

## One‑Liner Sweeps (PowerShell)

Sweep α/β to map modulation envelopes and correlations:

```
$alphas = @(0.0, 0.4, 0.8)
$betas  = @(0.0, 0.4, 0.8)
foreach ($a in $alphas) {
  foreach ($b in $betas) {
    $tag = "a${a}_b${b}"
    $json = "PhaseC_Logs/telemetry_${tag}.jsonl"
    ./neuroforge.exe --phase-c=on --phase-c-mode=binding `
      --phase-c-survival-bias=on `
      --audio-demo=on `
      --steps=600 --step-ms=50 `
      --phase-c-hazard-alpha=$a `
      --phase-c-hazard-beta=$b `
      --log-json=$json `
      --log-json-events=C:survival_mod
    python scripts/phase_c_compute_rollups.py `
      --events $json `
      --max-lag 200 `
      --out-csv "PhaseC_Logs/rollups_${tag}.csv"
  }
}
```

## Lab Notebook Template

Copy/paste and fill for each run:

```
Run ID: phasec_audio_a0.8_b0.4_seed1234
Timestamp: 2025-10-16T15:30:00Z
Executable: C:\Users\ashis\Desktop\NeuroForge\build-ninja\neuroforge.exe
Seed: 1234
Task: Phase C (binding)
Steps / Step-ms: 600 / 50
Flags:
  phase-c-survival-bias=on
  audio-demo=on
  phase-c-hazard-alpha=0.8
  phase-c-hazard-beta=0.4
  log-json-events=C:survival_mod
Artifacts:
  JSONL: PhaseC_Logs\telemetry_alpha_beta.jsonl
  CSV: assemblies.csv, bindings.csv, timeline.csv, working_memory.csv
Analysis:
  Rollups CSV: PhaseC_Logs\rollups_audio.csv
  sigma_effective_weight: <value>
  corr_effective_reward (peak / lag): <value> / <lag>
  Hazard stats: <summary>
Notes:
  Observed effective_weight range: <min..max>
  Remarks on environmental coupling and coherence changes.
```

## Interpretation Notes
- `effective_weight` reflects the applied modulation after α/β scaling and any clamps; under fixed hazard with α=β=0, it should match the base `weight`.
- Audio RMS hazard introduces variability step‑to‑step; look for corresponding changes in `modulated` and `delta`.
- Compare `sigma_effective_weight` and lag peaks between fixed vs. audio runs to characterize adaptivity.

## Next Steps
- Extend sweeps to hazard densities and WM decay for broader envelopes.
- Consider a preset flag (e.g., `--telemetry-preset=adaptive`) to bundle α/β, audio hazard, and filters.
- Optionally add a `scripts/run_phase_c_experiment.py` wrapper to orchestrate runs and rollups.

This script computes stability and coupling metrics from Phase C telemetry, producing a compact CSV/JSON summary suitable for sweeps and benchmarking.

## Usage

Run on one or more events logs, optionally with a MemoryDB for reward:

```
python scripts/phase_c_compute_rollups.py \
  --events ./PhaseC_Logs/events.json ./PhaseC_Logs/events_audio.json \
  --memory-db ./neuroforge_memory.db \
  --max-lag 200 \
  --out-csv ./PhaseC_Logs/phase_c_long_rollups.csv \
  --out-json ./PhaseC_Logs/phase_c_long_rollups.json
```

Outputs are written to the paths specified by `--out-csv` and `--out-json`.

## Columns

- `file`: input telemetry file name.
- `condition`: inferred from file name (`audio` vs `fixed`).
- `has_survival_mod`, `count_survival_mod`: presence and count of `survival_mod` events.
- `sigma_effective_weight`: population σ of `effective_weight` across `survival_mod` events.
- `sigma_modulated`: population σ of `modulated` strength across `survival_mod` events.
- `sigma_binding_strength`: population σ of binding `strength` (useful when audio path lacks `survival_mod`).
- `hazard_probability_mean`, `hazard_probability_min`, `hazard_probability_max`: summary of hazard probability from `survival_mod` events.
- `corr_effective_reward`: Pearson correlation between co-timestamped `effective_weight` and reward from MemoryDB.
- `lag_peak`, `corr_at_peak`: cross-correlation peak lag (in steps) and its correlation value.

## Notes and Interpretation

- `effective_weight` fallback: if absent, the script uses `weight` when available.
- Audio path: in pure audio-hazard mode today, `survival_mod` may be absent; stability can be assessed via `sigma_binding_strength` until telemetry is aligned.
- Reward optional: if `--memory-db` is omitted or the table is missing, correlation and lag fields are blank.
- Lag sign convention: positive `lag_peak` means `effective_weight` leads reward by `lag_peak` steps (eff at `s-lag` vs reward at `s`); negative means reward leads.
- Heuristics:
  - Lower `sigma_effective_weight` indicates higher modulation stability; moderate variability (e.g., ≈0.02) suggests adaptive movement without thrashing.
  - High absolute `corr_effective_reward` and a clear `lag_peak` support coupling and causal direction.

## Workflow Recommendations

- Use alongside `scripts/dashboard_modulation.py` for visual sanity checks.
- Sweep `alpha`, `beta`, and hazard-density; collect rollups per run to build a benchmark table.
- Compare fixed hazard and audio-driven runs once audio `survival_mod` telemetry is enabled.

## Correlation Metrics

- Default axis: prefer `corr_at_peak` for Phase C sweeps; it scans lags and avoids sample loss when reward and control are asynchronous.
- Zero-lag `corr_effective_reward`: use when reward emission is synchronized with the control signal, or when you increase `--steps` and adjust hazard timing to improve alignment.
- Interpretation: `corr_at_peak` is robust to timing offsets; `corr_effective_reward` requires co-timestamped pairs and returns blank/None when overlap is insufficient or the effective-weight series has near-zero variance.
- Pipeline defaults: the stats filter tool (`scripts/filter_stats.py`) defaults to `corr_at_peak` with hazard=`audio` and mode=`binding` for reliable per-scale reporting.
- Aggregation example: `python scripts/aggregate_rollups.py PhaseC_Logs --out PhaseC_Logs/summary.csv --out-json PhaseC_Logs/summary.json --stats-out PhaseC_Logs/stats_corr_peak.csv --stats-out-json PhaseC_Logs/stats_corr_peak.json --stats-group-by survival_reward_scale --stats-x sigma_effective_weight --stats-y corr_at_peak --stats-filter-hazard audio --stats-filter-mode binding`.
- Aggregation (windowed zero-lag): `python scripts/aggregate_rollups.py PhaseC_Logs --out PhaseC_Logs/summary.csv --out-json PhaseC_Logs/summary.json --stats-out PhaseC_Logs/stats_corr_zero_lag_windowed.csv --stats-out-json PhaseC_Logs/stats_corr_zero_lag_windowed.json --stats-group-by survival_reward_scale --stats-x sigma_effective_weight --stats-y corr_zero_lag_windowed --stats-filter-hazard audio --stats-filter-mode binding`.
- Windowed zero-lag `corr_zero_lag_windowed`: computes a weighted mean of Pearson r over lags in ±N around zero, mitigating sparse overlap. Enable via `--zero-lag-window N`; the value is emitted as `corr_zero_lag_windowed` in rollups.
- Example: `python scripts/phase_c_compute_rollups.py --events PhaseC_Logs/exp_.../telemetry.jsonl --memory-db path\\to\\db.sqlite --zero-lag-window 5 --out-csv PhaseC_Logs/exp_.../rollups.csv --out-json PhaseC_Logs/exp_.../rollups.json`
- Improving zero-lag: raise `--steps` (≥600–1000), tighten hazard emission timing, or add a windowed zero-lag option that averages correlation over ±N steps instead of strict alignment.