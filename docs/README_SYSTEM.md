# NeuroForge System Guide

This guide captures the Phase 6–15 cognitive-ethical pipeline: CLI flags, exporter, simulator, and dashboard workflows. Use it to run end-to-end regressions and avoid common serving pitfalls.

## CLI Phases (neuroforge.exe)
- `--unified-substrate=on` Enables the integrated architecture (WM + Phase C + Language + Survival).
- `--gpu` Enables CUDA acceleration for learning updates.
- `--phase6` Learning
- `--phase7` Reflection
- `--phase8` Goal Alignment
- `--phase9` Metacognition Core
- `--phase10` Self-Explanation
- `--phase11` Self-Revision
- `--phase12` Consistency
- `--phase13` Autonomy Envelope
- `--phase14` Meta-Reasoner
- `--phase15` Ethics & Constraint Regulator

Example combined run:

`build\neuroforge.exe --phase6 --phase7 --phase8 --phase9 --phase10 --phase11 --phase12 --phase13 --phase14 --phase15`

## Exporter Workflow
- Export from a SQLite memory DB to a unified JSON for the dashboard.
- Supports DB-first integration with simulator JSON fallback for Phase 15.

Commands:
- Latest run: `python .\scripts\dump_metacognition.py --db .\web\phasec_mem.db --out .\web\metacognition_export.json`
- Specific run: `python .\scripts\dump_metacognition.py --db .\web\phasec_mem.db --run-id 6 --out .\web\metacognition_export.json`

Notes:
- The exporter writes a timestamped file and also a plain copy to `web\metacognition_export.json` for the dashboard.
- Ethics regulator data is merged into the unified export under `ethics_regulator_log`.

## Unified Self System Overview
- Identity snapshots are persisted in `self_concept` (`run_id, ts_ms, step, identity_vector_json, confidence, notes`).
- Personality revisions are recorded in `personality_history` (`run_id, ts_ms, step, trait_json, proposal, approved, source_phase, revision_id, notes`).
- Phase 11 emits personality revision proposals only (`proposal=1, approved=0`) into `personality_history`.
- A centralized approval gate in `MemoryDB` (`approvePersonalityProposal`) flips `approved=1` for selected proposals and appends audit notes.
- Phase 15 (`Phase15EthicsRegulator`) can act as an approval authority by evaluating risk and, when explicitly invoked, calling the approval gate for a given `personality_history.id`.
- `SelfModel` and dashboards can consume `self_concept` and approved `personality_history` rows to derive identity metrics (e.g., awareness, confidence, identity drift).

## Simulator Workflow (Phase 15)
- Generate realistic ethics decisions if DB is not populated.

`python .\scripts\simulate_phase15_ethics.py --out .\web\ethics_regulator_log.json --events 300 --risk-threshold 0.30`

Key fields: `ts_ms`, `decision`, `risk`, `notes`, `context` (with `coherence`, `goal_mae`, `window`, `threshold`).

## Dashboard Workflow
1) Serve from the `web` directory:
   - `python -m http.server 8000` (cwd: `c:\Users\ashis\Desktop\NeuroForge\web`)
   - Open: `http://localhost:8000/phase9.html`
2) Alternatively, serve from repo root:
   - `python -m http.server 8000` (cwd: `c:\Users\ashis\Desktop\NeuroForge`)
   - Open: `http://localhost:8000/web/phase9.html`

The dashboard loads `metacognition_export.json` and will optionally fetch `ethics_regulator_log.json` if present. `304` responses indicate cache hits and are fine.

## Server Root Note
- If serving from `web`, do not include `web/` in paths (`http://localhost:8000/phase9.html`).
- If serving from repo root, include `web/` (`http://localhost:8000/web/phase9.html`).
- A `404` typically means the server’s root is not aligned with the path being requested.

## Smoke Tests
- Phase 15 threshold sweep: `pytest -q tests/test_phase15_shift.py`
- Ensures “allow” decisions increase as `--risk-threshold` rises.

## End-to-End Regression
1) Run phases: `build\neuroforge.exe --phase6 --phase7 --phase8 --phase9 --phase10 --phase11 --phase12 --phase13 --phase14 --phase15`
2) Export: `python .\scripts\dump_metacognition.py --db .\web\phasec_mem.db --out .\web\metacognition_export.json`
3) Open dashboard, confirm all panels update over time.

## Autonomy Envelope Observation Runs (Stage 6.5)
- Stage 6.5 implements a read-only autonomy envelope that fuses identity confidence, `self_trust`, ethics decisions, and social alignment into a continuous autonomy score and tier. It never gates actions directly; enforcement remains in the Unified Action Filter plus Phase 13 and Phase 15.
- A recommended governance-lit run template for observing autonomy dynamics on an existing M1 triplet grounding task is:

```powershell
& ".\build\neuroforge.exe" `
  --steps=10000 `
  --step-ms=10 `
  --dataset-triplets="c:\Users\ashis\Desktop\NeuroForge\flickr30k_triplets" `
  --dataset-mode=triplets `
  --dataset-limit=50 `
  --memory-db="build\m1_autonomy_observe_triplets.db" `
  --memdb-interval=500 `
  --phase-a=on `
  --phase5-language=on `
  --enable-learning `
  --mimicry=on `
  --mirror-mode=vision `
  --telemetry-extended=on `
  --vision-grid=16 `
  --student-learning-rate=0.01 `
  --reward-scale=1.5 `
  --phase-a-mimicry-repeats=3 `
  --negative-sampling-k=3 `
  --negative-weight=0.1 `
  --phase-a-similarity-threshold=0.05 `
  --phase-a-novelty-threshold=0.0 `
  --phase9 `
  --phase9-modulation=on `
  --phase13 `
  --phase15
```

- After the run, focus analysis on the following tables for Stage 7 readiness:
  - `learning_stats_run_<id>.csv`
  - `metacognition_run_<id>.csv`
  - `autonomy_envelope_log_run_<id>.csv`
  - `ethics_regulator_log_run_<id>.csv`
- Stage 7 progression should be considered only if autonomy changes smoothly, lags `self_trust` slightly, never leads `self_trust`, respects absolute ethics veto (hard blocks collapse autonomy to `NONE`), avoids unexplained jumps to `FULL`, and does not oscillate between `NONE` and `CONDITIONAL` in the absence of ethics changes.
- For quick DB-level checks of autonomy envelope and ethics dynamics, use:

```powershell
.\.venv\Scripts\python.exe .\scripts\analyze_autonomy_envelope.py build\m1_autonomy_observe_triplets.db
```

This script reports table row counts, samples `autonomy_envelope_log` entries (including `tier`, `autonomy_score`, `self_trust`, and `ethics_hard_block`), and verifies that `FULL` autonomy tiers respect the ethics veto.

## Version Tag & Archive (optional)
- Tag: `git tag v0.15.0 && git push origin v0.15.0`
- Archive: zip `web/`, `scripts/`, and `build/neuroforge.exe`
- Release: include screenshots of all panels.

## Live Telemetry Upgrade (optional)
- Add a lightweight WebSocket server emitting incremental updates from MemoryDB.
- Dashboard renderers support incremental redraws; only event hooks are needed.

## Sandbox Agent + Foveation (New)
- Launch a sandboxed window and capture only its client area with dynamic fovea.
- Use YouTube preset to couple screen vision with system audio for rewards.

Example: sandboxed YouTube agent

`build-vcpkg-msvc\Release\neuroforge.exe --vision-demo --motor-cortex --sandbox=on --sandbox-url=https://www.youtube.com --audio-demo --audio-system=on --steps=600`

Example: attention-guided foveation

`build-vcpkg-msvc\Release\neuroforge.exe --vision-demo --motor-cortex --sandbox=on --sandbox-url=https://www.youtube.com --audio-demo --audio-system=on --foveation=on --fovea-mode=attention --fovea-size=640x360 --fovea-alpha=0.3 --steps=600`

Flags
- `--sandbox[=on|off]` enable sandbox window
- `--sandbox-url=URL` initial navigation (default `https://www.youtube.com`)
- `--sandbox-size=WxH` client area size (e.g., `1280x720`)
- `--foveation[=on|off]` dynamic retina focusing
- `--fovea-size=WxH` fovea rectangle size
- `--fovea-mode=cursor|center|attention` fovea center selection
- `--fovea-alpha=F` EMA smoothing in `[0,1]`

Telemetry
- JSON event logs include `vision.retina` `{x,y,w,h}` and `vision.foveation` `{enabled,mode,alpha}`.

### Binary Selection Note
- Prefer `build-vcpkg-msvc\Release\neuroforge.exe` for sandbox runs; this build includes WebView2 integration and documented sandbox flags in `--help`.
- Quick verification: `& .\build-vcpkg-msvc\Release\neuroforge.exe --help | Select-String sandbox`

### Sandbox Startup Stability (New)
- The sandbox uses an init wait phase to ensure readiness before the run loop.
- Readiness includes: controller creation, initial navigation starting, and one bounds update.
- Benefit: Prevents hangs in runs where actions are disabled or delayed.

## Sandbox Agent + Foveation (New)
- Enable a sandboxed window for safe interaction and optional embedded browser.
- Focus the screen capture on a high‑value region using dynamic foveation.

### Quick Start
```
./neuroforge.exe --vision-demo --motor-cortex --sandbox=on --sandbox-url=https://www.youtube.com \
  --audio-demo --audio-system=on --steps=600
```

### Attention‑guided Foveation
```
./neuroforge.exe --vision-demo --motor-cortex --sandbox=on --sandbox-url=https://www.youtube.com \
  --audio-demo --audio-system=on --foveation=on --fovea-mode=attention --fovea-size=640x360 --fovea-alpha=0.3 \
  --steps=600
```

### Flags
- `--sandbox[=on|off]` enables the sandbox window
- `--sandbox-url=URL` sets initial navigation (default `https://www.youtube.com`)
- `--sandbox-size=WxH` sets client area
- `--foveation[=on|off]` enables dynamic retina focusing
- `--fovea-size=WxH` sets the fovea rectangle size
- `--fovea-mode=cursor|center|attention` chooses the center source
- `--fovea-alpha=F` EMA smoothing `[0,1]`

### Telemetry
- JSON logs include `vision.retina` (active capture rect) and `vision.foveation` (config).
- See `docs/telemetry.md` for field details and examples.

### Action Gating Overview (New)
- All actions are filtered by a unified gating layer with explicit reasons.
- Reasons include: `ok`, `no_web_actions`, `phase15_deny`, `phase13_freeze`.
- Reasons appear in action payloads and reward context for reproducibility.

