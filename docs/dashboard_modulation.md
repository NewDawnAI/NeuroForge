Dashboard: Modulation vs Reward/Consolidation

This helper script visualizes adaptive hazard modulation alongside reward and consolidation events.

- Inputs: line-delimited JSON events (`survival_mod`, `consolidation`) and optional MemoryDB SQLite (`reward_log`).
- Output: a PNG overlay of `effective_weight`, `reward` time series, and vertical markers for `consolidation`.

Usage
- Default run using Phase C JSON events:
  - `./.venv/Scripts/python.exe scripts/dashboard_modulation.py --events ./PhaseC_Logs/events.json --out ./PhaseC_Logs/dashboard.png`
- Include MemoryDB reward overlay:
  - `./.venv/Scripts/python.exe scripts/dashboard_modulation.py --events ./PhaseC_Logs/events.json --memory-db analysis/telemetry/phase_c_run_learn.db --reward-table reward_log --out ./PhaseC_Logs/dashboard.png`

Notes
- The script gracefully handles missing data: if reward or consolidation are unavailable, it plots whatever is present.
- `effective_weight` is read from `survival_mod` events. If only `weight` is present, it falls back to that value.
- Use `--title` to customize plot title.

Why this matters
- Quickly inspect modulation stability (`effective_weight` variance).
- Assess coupling between modulation and outcomes (reward spikes, consolidation timing).
- Validate that adaptive scaling (α/β) is active and reflected in telemetry.