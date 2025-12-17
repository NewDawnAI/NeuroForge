# End-to-End Regression Run

This document outlines the end-to-end regression for Phases 6–15, exporting telemetry and validating the dashboard.

## Steps
1) Run all phases:

`build\neuroforge.exe --phase6 --phase7 --phase8 --phase9 --phase10 --phase11 --phase12 --phase13 --phase14 --phase15`

2) Export unified telemetry:

`python .\scripts\dump_metacognition.py --db .\web\phasec_mem.db --out .\web\metacognition_export.json`

3) Serve dashboard from `web`:

`python -m http.server 8000` (cwd: `c:\Users\ashis\Desktop\NeuroForge\web`)

Open `http://localhost:8000/phase9.html`

4) Alternatively, serve from repo root:

`python -m http.server 8000` (cwd: `c:\Users\ashis\Desktop\NeuroForge`)

Open `http://localhost:8000/web/phase9.html`

## Validation Checklist
- Panels for Phases 9–15 render without errors.
- `ethics_regulator_log` appears in the unified export (DB-first; simulator fallback).
- Timeline and frequency views update as data changes.
- No dangling fetches or 404s.
- Action gating reasons present:
  - `actions.payload_json.filter.reason` ∈ { `ok`, `no_web_actions`, `phase15_deny`, `phase13_freeze` }.
  - `reward_log.context_json.blocked_actions.*` counters increment when gated.
- Sandbox readiness verified: window initializes, first navigation starting fires, bounds update occurs before run loop.

## Useful Commands
- Latest run export: `python .\scripts\dump_metacognition.py --db .\web\phasec_mem.db --out .\web\metacognition_export.json`
- Specific run export: `python .\scripts\dump_metacognition.py --db .\web\phasec_mem.db --run-id 6 --out .\web\metacognition_export.json`
- Phase 15 simulator: `python .\scripts\simulate_phase15_ethics.py --out .\web\ethics_regulator_log.json --events 300 --risk-threshold 0.30`
- Smoke test: `pytest -q tests/test_phase15_shift.py`

### Synthetic Gating & Reward Debug Run
```powershell
& .\neuroforge.exe --steps=50 --step-ms=10 ^
  --simulate-blocked-actions=5 --simulate-rewards=3 ^
  --memory-db .\nf_test.db --memdb-interval=10 --log-shaped-zero=on
```
