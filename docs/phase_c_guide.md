# Phase C Guide — Runs, Telemetry, and Analysis

This guide covers running Phase C (Global Workspace with role–filler binding), capturing telemetry, and performing analysis and correlation validation.

## Overview
- Phase C produces CSV logs for winners, assemblies, bindings, and working memory.
- Telemetry writes to a SQLite MemoryDB for reward and learning metrics.
- Python tools generate plots, summaries, and correlation validation JSON.

## Run Setup
- Binary: `neuroforge.exe` with Phase C flags, or existing harnesses that emit Phase C CSVs.
- **Unified Substrate (Recommended)**:
```powershell
.\neuroforge.exe --unified-substrate=on --gpu --enable-learning --steps=1000 --memdb-interval=50
```
- Typical standalone binding run (legacy):
```powershell
.\neuroforge.exe --phase-c=on --phase-c-mode=binding --enable-learning ^
  --steps=640 --stdp-rate=0.002 --consolidation-interval=16 --consolidation-strength=0.4
```

## Hazard Modulation (SurvivalBias)
- Enable risk-driven coherence modulation with `--phase-c-survival-bias[=on|off]`.
- Tune sensitivity to activation variance with `--phase-c-variance-sensitivity=F` (positive float).
- Inject an external hazard signal with `--hazard-density=F` where `F` is in `[0,1]`.
  - If `F > 0`, this fixed density overrides any audio-derived hazard.
  - If `F = 0` and `--audio-demo` is enabled, the system uses normalized audio RMS as hazard (mic‑fed fallback).
  - If `F = 0` and audio is disabled, no external hazard override is applied.
- Down‑modulation weight is configurable via `--phase-c-hazard-weight=F` (default `0.2`, clamped to `[0,1]`).
- Optional adaptive scaling: enable dynamic modulation using
  - `--phase-c-hazard-alpha=F` in `[0,1]` for sensitivity to external hazard
  - `--phase-c-hazard-beta=F` in `[0,1]` for sensitivity to internal arousal
  - Applied modulation is `effective_weight = weight * clamp(alpha * external_hazard + beta * arousal_level, 0, 1)`.
  - Defaults `alpha=0`, `beta=0` preserve baseline behavior (`effective_weight == weight`).
- SurvivalBias reduces assembly coherence proportional to risk; modulation is applied inside the Phase C loop each step.
 - Telemetry includes `hazard_probability`, `risk_score`, `arousal_level`, `weight` (configured down‑modulation weight), and `effective_weight` (the applied modulation weight after any overrides).

Examples:
```powershell
# Fixed hazard density driving SurvivalBias
.\neuroforge.exe --phase-c --phase-c-mode=binding --phase-c-survival-bias=on ^
  --hazard-density=0.5 --phase-c-hazard-weight=0.20 --steps=50 --log-json=on

# Audio-driven hazard (microphone) modulating coherence
.\neuroforge.exe --phase-c --phase-c-mode=sequence --phase-c-survival-bias=on ^
  --audio-demo --audio-mic=on --audio-samplerate=16000 --hazard-density=0 ^
  --phase-c-hazard-weight=0.15 --steps=50 --log-json=on
```

## Telemetry Capture
Set environment variables to enable MemoryDB telemetry during runs:
```powershell
$env:NF_TELEMETRY_DB = "C:\Users\ashis\Desktop\NeuroForge\phasec_mem.db"
$env:NF_ASSERT_ENGINE_DB = "1"            # seeds initial rows and asserts telemetry presence
$env:NF_MEMDB_INTERVAL_MS = "50"           # periodic learning_stats sampling interval
```
- CLI `--memdb-interval=MS` overrides `NF_MEMDB_INTERVAL_MS`.
- Telemetry files are created if missing and appended across runs.
### Survival Rewards Emission
Phase C can emit survival rewards that reflect hazard-driven coherence modulation.

Notes:
- Emission is controlled inside the substrate adapter; default is disabled.
- When enabled, `reward_log` rows appear with `source='phase_c_survival'`.
- Scale with `--phase-c-survival-scale=F` (e.g., `0.8`) to adjust reward magnitude.
- Combine with `--phase-c-survival-bias=on` and hazard inputs for meaningful signals.

### Unified Reward Sources (Shaped/Merged/Survival)
- Shaped reward combines teacher similarity, novelty, and survival components:
  `shaped = (teacher * wt_T) + (novelty * wt_N) + (survival * wt_S)`.
- Configure weights via `--wt-teacher`, `--wt-novelty`, `--wt-survival`.
- `reward_log` records `source='shaped'`, `source='merged'` (arbiter output), and `source='survival'` when enabled.
- Use `--log-shaped-zero=on` to emit shaped rows even when components are zero (debugging).

## Example: Long Run with Survival Rewards and Learning
```powershell
# Start a long Phase C run with survival rewards and learning enabled
$env:NF_TELEMETRY_DB = "C:\Users\ashis\Desktop\NeuroForge\phasec_mem.db"
$env:NF_ASSERT_ENGINE_DB = "1"
& ".\build-vcpkg-vs\Release\neuroforge.exe" ^
  --phase-c=on ^
  --phase-c-mode=binding ^
  --enable-learning ^
  --phase-c-survival-bias=on ^
  --phase-c-survival-scale=0.8 ^
  --phase-c-hazard-weight=0.2 ^
  --hazard-density=0.6 ^
  --memdb-interval=250 ^
  --steps=60000 ^
  --step-ms=20 ^
  --log-json=on ^
  --log-json-events=C:reward,C:consolidation
```

## Run Logs (CSV)
Expect the following in the run’s logs directory:
- `timeline.csv` — step, winner_id, winner_symbol, winner_score
- `assemblies.csv` — step, assembly_id, symbol, score
- `bindings.csv` — step, role, filler, strength
- `working_memory.csv` — WM state (capacity, items, decay) if enabled

## Analysis Workflow
Generate plots and a textual summary from the logs:
```powershell
python .\scripts\phase_c_analyze_run.py --logs-dir "C:\Users\ashis\Desktop\NeuroForge\analysis\phase_c\run_learn_new"
```
Outputs saved to the same folder:
- `winner_timeline.svg` — winner symbol over time
- `winner_frequency.svg` — histogram of winner occurrences
- `assembly_richness.svg` — assemblies richness across steps
- `summary.txt` — stats for timeline/assemblies/bindings

## Correlation Validation (run-filtered)
Compute Pearson correlations of reward vs learning metrics for a specific run and perform lag analysis:
```powershell
python .\scripts\phase_c_compute_correlations.py ^
  --db "C:\Users\ashis\Desktop\NeuroForge\phasec_mem.db" ^
  --run-id 6 ^
  --out "C:\Users\ashis\Desktop\NeuroForge\analysis\phase_c\phase_c_validation_run6_latest.json"
```
Produces:
- `phase_c_validation_run6_latest.json` — `reward_events` count, `reward_vs_avg_weight_change_r`, and best lag for `consolidation_rate`.

## Database Inspection
Inspect schema and sampled rows from the telemetry DB:
```powershell
python .\scripts\db_inspect.py --db "C:\Users\ashis\Desktop\NeuroForge\phasec_mem.db" --limit-json 1
```
Shows columns for `reward_log`, `learning_stats`, `episodes`, `episode_experiences`, `episode_stats`, `self_model`, `substrate_states`, and `hippocampal_snapshots`.

## Export Run-Specific CSVs
```powershell
# Export learning_stats and reward_log for a given run_id
python .\scripts\export_phasec_csv.py "C:\Users\ashis\Desktop\NeuroForge\phasec_mem.db" ^
  --run-id 6 ^
  --out-dir "C:\Users\ashis\Desktop\NeuroForge\analysis\phase_c\db_exports\run_6" ^
  --tables "learning_stats,reward_log"
```
Outputs:
- `learning_stats_run_6.csv`, `reward_log_run_6.csv` written to the out-dir.

## Common Pitfalls
- `consolidation_rate` constant or zero → correlation is `NaN`. Lag search still returns `best_lag`.
- Ensure `pandas` and `matplotlib` are installed to generate plots (`pip install pandas matplotlib`).
- Windows PowerShell: always invoke Python as `python path\to\script.py`.

## Artifact Locations (example)
- Logs: `C:\Users\ashis\Desktop\NeuroForge\analysis\phase_c\run_learn_new\`
- Plots/Summary: saved alongside the CSV logs
- Telemetry DB: `C:\Users\ashis\Desktop\NeuroForge\phasec_mem.db`
- Validation JSON: `C:\Users\ashis\Desktop\NeuroForge\analysis\phase_c\phase_c_validation_run6_latest.json`

## Next Steps
- Extend telemetry to include additional substrate metrics (e.g., `global_activation`, `coherence`).
- Run cross‑run comparisons using multiple DBs and aggregate statistics.
