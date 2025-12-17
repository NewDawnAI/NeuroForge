# Phase 17a – Context Hooks Experiment Design

## Overview
Phase 17a introduces environment-aware modulation of the Ethics Regulator (Phase 15) via ContextHooks. Each decision is influenced by an exogenous context signal with configurable gain (κ), update cadence, and memory window. This enables contextual alignment analysis across sensitivity and persistence dimensions.

## Parameters
- `--context-gain` (`κ`): real number ≥ 0; scales how strongly context shifts perceived risk.
- `--context-update-ms` (`update_ms`): integer in milliseconds; cadence for context sampling updates.
- `--context-window` (`window`): integer; number of recent samples used to compute context memory.

### Recommended Sweep Ranges
- Gain: `0.5 … 2.0` in steps of `0.25`
- Update Cadence: `50 … 1000 ms`
- Window: `3 … 20`
- Freeze Baseline: `--context-gain 0.0` (disables context impact)

## Telemetry Schema
Context telemetry is recorded to MemoryDB and embedded in JSON snapshots.

### MemoryDB Tables
- `context_log`
  - `id INTEGER PRIMARY KEY`
  - `run_id INTEGER`
  - `ts_ms INTEGER` – monotonic timestamp
  - `sample REAL` – context value
  - `gain REAL` – κ applied at decision time
  - `update_ms INTEGER`
  - `window INTEGER`
  - `label TEXT` – source label provided to the sampler

- `ethics_regulator_log`
  - `id INTEGER PRIMARY KEY`
  - `run_id INTEGER`
  - `ts_ms INTEGER`
  - `decision TEXT` – one of `allow|review|deny`
  - `driver_json TEXT` – structured explanation (includes risk, thresholds, and recent context)

### JSON Embedding
- Shaped reward telemetry block includes:
  - `context_cfg`: `{ gain, update_ms, window }`
  - `context_recent`: array of `{ ts_ms, sample, label }`
- Experience snapshot extended metadata includes the same context fields.

## First-Order Effects
- Risk Modulation: Effective risk `r_eff = r_base + κ * context_sample`.
- Inertia Shift: Larger `window` increases decision inertia; smaller window increases responsiveness.
- Cadence Impact: Faster `update_ms` tracks rapid environmental changes; slower cadence smooths fluctuations.
- Stability Coupling: Interacts with Phase 16 noise (`σ_n`); expect σ-max and τ to vary with κ.

## Suggested Experiments (17a–b)
1. Context Sensitivity Sweep — map ethics drift vs gain
   - `--context-gain 0.5 … 2.0 step 0.25`
2. Update Cadence Sweep — detect reaction-time limits
   - `--context-update-ms 50 … 1000`
3. Context Persistence Study — tune inertia coupling
   - `--context-window 3 … 20`
4. Context Freeze Test — baseline (disable change)
   - `--context-gain 0.0`
5. Noise × Context Interaction — combine Phase 16 noise with κ
   - `--context-gain 1.25 --risk-noise-std 0.04`

## Validation Checklist
- DB sanity:
  - `SELECT * FROM context_log LIMIT 10;`
  - Expect fields: `ts_ms, sample, gain, update_ms, window, label`.
- Exporter:
  - `python scripts/dump_metacognition.py --db web/phasec_mem.db --out web/metacognition_export_ctx.json`
  - Verify `context` blocks appear alongside `ethics_regulator_log`.
- Dashboard:
  - Phase 15 page shows Context Stream and decision ratios.
  - Tooltip `context_cfg` helps interpret gain/cadence impact.
- Performance:
  - Long runs with `scripts/collect_sys_stats.py` show < 2% CPU overhead from ContextHooks.

