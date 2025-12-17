# NeuroForge How-To Demo (M1–M4)

This document collects short, copy-pasteable run examples for Milestones M1 through M4 on Windows (PowerShell). Use .\build\Release\neuroforge.exe unless noted. All commands are non-interactive.

Note: Available flags are described in the binary’s help and main options; see build\help.txt and src\main.cpp for authoritative reference.

Telemetry quick checks:
- `NF_TELEMETRY_DB` sets the SQLite DB path for periodic logging.
- `NF_ASSERT_ENGINE_DB=1` seeds telemetry rows and asserts presence for short runs.
- `NF_MEMDB_INTERVAL_MS` sets periodic logging interval; `--memdb-interval` overrides env.

## Demo Script (60–90s) — Phase C Live Dashboard
A tight sequence to demo the live dashboard with realtime JSON events.

1) Install deps (first time only):

PowerShell
python -m pip install Flask flask-socketio eventlet

2) Launch the dashboard (port 8020) with a recognizable brand color and no artifact writes:

PowerShell
python .\scripts\flask_app.py --port 8020 --preset strength --brand-color "#FF5733" --no-artifacts

3) Open the dashboard:
- http://localhost:8020/
- You should immediately see Live JSON Events begin to populate (config, scale, panel_best, stderr when relevant).
- The heatmap outline moves as panel_best updates (UI debounced ~200 ms).
- The metric sparkline updates as scale events arrive.

4) Show interactivity (10–15s):
- Change Preset from strength → accuracy (producer restarts with the new shorthand; events continue).
- Toggle --no-artifacts on/off (for the demo, keep it on to avoid PNG writes).

5) Close with the takeaway:
- “Realtime telemetry is plumbed end-to-end; we can swap WS/SSE transport seamlessly in constrained environments.”

---

## Telemetry — Smoke and Fast Periodic Logging

Smoke (seeded telemetry, 5 steps):

PowerShell
 $env:NF_ASSERT_ENGINE_DB = "1"
 $env:NF_TELEMETRY_DB = (Resolve-Path ".\build\assertion_smoke.db").Path
 .\build\Release\neuroforge.exe --steps=5

Fast periodic logging (25 ms interval, 50 steps):

PowerShell
 $env:NF_TELEMETRY_DB = (Resolve-Path ".\build\interval_test.db").Path
 $env:NF_MEMDB_INTERVAL_MS = "25"
 .\build\Release\neuroforge.exe --steps=50 --step-ms=1

CLI precedence example:

PowerShell
 $env:NF_MEMDB_INTERVAL_MS = "50"
.\build\Release\neuroforge.exe --memdb-interval=10 --steps=50

## MSVC Telemetry — Longer Run + Engine ↔ Agent Bridge
Objective: Produce a 200-step telemetry DB with seeded rows, inspect tables, and stream reward_log into the Phase C agents.

1) Longer telemetry run (200 steps):

PowerShell
 $env:NF_ASSERT_ENGINE_DB = "1"
 $env:NF_TELEMETRY_DB = (Resolve-Path ".\vs_smoke_long200.db").Path
 .\build-vcpkg-vs\Release\neuroforge.exe --steps=200 --step-ms=1

Notes:
- If your primary build output is under `.\build\Release`, replace the path accordingly: `.\build\Release\neuroforge.exe --steps=200 --step-ms=1`.
- `NF_ASSERT_ENGINE_DB=1` seeds the initial `reward_log` and asserts DB presence for short/medium runs.

2) Inspect the DB quickly:

PowerShell
 python .\scripts\inspect_db.py --db .\vs_smoke_long200.db --tables reward_log experiences self_model --tail 20

3) Live engine ↔ agent bridge (console demo):

PowerShell
 python -m build.out.engine_bridge --db .\vs_smoke_long200.db --interval 0.25

Optional: Full Phase C loop coherence (planner + language):

PowerShell
 python .\scripts\demo_phase_c_bridge.py --db .\vs_smoke_long200.db --duration 3 --interval 0.25

What to expect:
- Console prints of `[plan]` and `[narrative]` lines as the planner reacts to `winner` messages.
- Reward events flow as `payload.winner_symbol` and `payload.winner_score` derived from `reward_log.source` and `reward_log.reward`.
- Deterministic replay: re-running the 200-step smoke yields identical `reward_log` sequences.

Troubleshooting:
- `neuroforge.exe not found`: run from the repository root and adjust the path to your build output (`.\build\Release` or `.\build-vcpkg-vs\Release`).
- `ModuleNotFoundError: build.out`: ensure `.\build\out` contains `engine_bridge.py` and `phase_c_workspace.py`.
- `sqlite3.OperationalError: no such table reward_log`: confirm `NF_TELEMETRY_DB` is set and run `neuroforge.exe` with `NF_ASSERT_ENGINE_DB=1`.

## Language — Fast Non-Blocking Demo
Objective: Initialize the language system and exit immediately for CI/smoke.

PowerShell
 .\build-vcpkg\phase5_language_demo.exe --steps 1 --step_ms 0 --teacher_interval 0 --log_interval 1 --verbose

Notes:
- Disables step sleep and teacher cadence, guaranteeing termination after one step.
- For logs: add `--log_file .\build-vcpkg\language_demo.log`.
- Telemetry is best validated via `neuroforge.exe` or `test_memorydb.exe`; this demo focuses on fast init/exit.

## M1 — Phase A mirror mode (inject + readout, no learning)
Objective: Run Phase A in “mirror mode,” deriving the student embedding directly from sensory features or a provided teacher vector.

- Vision mirror (teacher vector matches grid^2 = 14×14 = 196)
  1) Create a teacher embedding of length 196:
     $p = 'experiments/milestone1/teacher_196.txt'; New-Item -Force -ItemType Directory (Split-Path $p) | Out-Null; (("0 " * 196).Trim()) | Set-Content -NoNewline $p
  2) Run Phase A mirror mode (vision):
     .\build\Release\neuroforge.exe --enable-language --phase-a=on --mirror-mode=vision --vision-grid=14 --teacher-embed=experiments\milestone1\teacher_196.txt --log-json=on --steps=2

- Audio mirror (dimension auto-derives from audio-feature-bins):
  .\build\Release\neuroforge.exe --enable-language --phase-a=on --mirror-mode=audio --audio-feature-bins=128 --log-json=on --steps=2

Tips:
- Use --log-json=on to print JSON events to stdout and verify decided_dim and mirror_mode.
- For a dimension conflict demo, provide a teacher vector of length 512 while vision grid is 14→196.

## M2 — Reward-modulated plasticity in substrate (mimicry-internal)
Objective: Route Phase A similarity/novelty internally via LearningSystem and confirm learning telemetry.

- Milestone-2 smoke (from experiments manifest):
  .\build\Release\neuroforge.exe --phase-a=on --mimicry-internal=on --steps=200 --step-ms=1 --log-json=experiments\milestone2\events.jsonl

Notes:
- --mimicry-internal routes Phase A signals into the substrate. Check experiments\milestone2\events.jsonl afterward.

## M3 — Internalize Phase A scoring/control (learning + live telemetry)
Objective: Enable learning and inspect substrate dynamics and connectivity snapshots.

- Learning with live snapshots and viewer:
  .\build\Release\neuroforge.exe --steps=400 --step-ms=1 --enable-learning --snapshot-live=.\live_synapses.csv --spikes-live=.\live_spikes.csv --viewer=on --viewer-layout=shells --viewer-refresh-ms=1000 --viewer-threshold=0.0

- Lightweight learning run (no viewer):
  .\build\Release\neuroforge.exe --steps=300 --step-ms=5 --enable-learning --log-json=on

Notes:
- You can post-process live_synapses.csv to visualize weight spectra or feed a watcher script if desired.

## M4 — Phase C (variable-binding and sequence)
Objective: Run the Phase C binding/sequence demos; logs are emitted to CSVs under the specified output directory.

- Binding demo (snapshot example):
  .\build\Release\neuroforge.exe --phase-c=on --phase-c-mode=binding --phase-c-out=Lab_Log\PhaseC_Snapshots\C0_binding --phase-c-seed=1234 --steps=80

- Sequence demo (snapshot example):
  .\build\Release\neuroforge.exe --phase-c=on --phase-c-mode=sequence --phase-c-out=Lab_Log\PhaseC_Snapshots\C0_sequence --phase-c-seed=1234 --steps=60

- Minimal sequence run (default output dir):
  .\build\Release\neuroforge.exe --phase-c=on --phase-c-mode=sequence --steps=60

- Parameter sweep harness (optional; generates per-run logs/plots and a summary CSV):
  python .\scripts\sweep_phase_c.py --capacities 3 6 12 --decays 0.7 0.85 0.95 1.0 --seq-windows 0 3 5 --mode sequence --plot --logdir .\build\Release\Sweeps

Outputs:
- Phase C runs write CSV logs and SVG plots under --phase-c-out (or default PhaseC_Logs). The sweep harness writes to build\Release\Sweeps and creates sweep_summary.csv with metrics across runs.

### Phase C — Consolidation JSON telemetry (machine-readable)
Emit compact JSON events on each consolidation cycle during Phase C.

PowerShell
 .\build\Release\neuroforge.exe --phase-c=on --phase-c-mode=sequence --enable-learning \
   --steps=150 --step-ms=1 --log-json=on --log-json-events=C:consolidation

Optional: persist to a file

PowerShell
 .\build\Release\neuroforge.exe --phase-c=on --phase-c-mode=sequence --enable-learning \
   --steps=150 --step-ms=1 --log-json=.\PhaseC_Logs\consolidation.jsonl --log-json-events=C:consolidation

Event payload fields:
- `count`: consolidation events since last step
- `total`: running total of consolidation events
- `rate`: `memory_consolidation_rate` from LearningSystem statistics
- `active_synapses`, `potentiated_synapses`, `depressed_synapses`: substrate learning stats

Reference: see `docs/HOWTO.md` → “Phase C Consolidation Telemetry” for more details.

## Environment and notes
- Run all commands from the repository root (C:\Users\ashis\Desktop\NeuroForge) so relative paths resolve.
- If neuroforge.exe is not in build\Release, build the project first, or set $env:NEUROFORGE_EXE to the full path to the binary.
- For help and all flags: .\build\Release\neuroforge.exe --help
 - Telemetry envs: set `NF_TELEMETRY_DB` and optionally `NF_MEMDB_INTERVAL_MS`; use `NF_ASSERT_ENGINE_DB=1` for smoke runs.

## Appendix — Phase C Live Dashboards (Realtime)
The repository includes two simple realtime dashboards for Phase C. Both stream newline-delimited JSON from the Python producer and render a small live UI.

- Option A (MVP, SSE-only): lightweight http.server dashboard on port 8010.
- Option B (Phase 2, Flask + Socket.IO): full dashboard on port 8020; automatically falls back to SSE if the Socket.IO client script is blocked.

### A) MVP SSE dashboard (port 8010)
Start the MVP dashboard and producer:

PowerShell
python .\scripts\phase_c_run.py --live --port 8010 --preset strength --brand-color "#FF5733" -Ps

Notes:
- The wrapper accepts extras with or without a standalone "--" separator; if provided, it will be stripped before launching the generator. Both of these work:
  - python .\scripts\phase_c_run.py --live --port 8010 --preset strength --brand-color "#FF5733" -Ps
  - python .\scripts\phase_c_run.py --live --port 8010 --preset strength -- --brand-color "#FF5733" -Ps
- Open http://localhost:8010/ and watch the Live JSON Events. You should see config, scale, panel_best, stderr (if any), and producer_done.
- The heatmap highlight moves when panel_best arrives (UI updates are lightly debounced).

### B) Flask + Socket.IO dashboard (port 8020)
This is the Phase 2 dashboard with WS + SSE fallback and simple controls.

Install (first time only):

PowerShell
python -m pip install Flask flask-socketio eventlet

Run the dashboard:

PowerShell
python .\scripts\flask_app.py --port 8020 --preset strength --brand-color "#FF5733" --no-artifacts

What to expect:
- Open http://localhost:8020/
- Preset dropdown restarts the producer with the selected metric shorthand.
- --no-artifacts prevents PNG writes by redirecting output to a null sink.
- If the Socket.IO client CDN is blocked in your environment, the page will automatically fall back to SSE and continue streaming events (this is expected in some embedded previews).
- Live panels mirror the MVP: config, scale, panel_best, stderr, producer_done.

### Troubleshooting
- No events or early exit with "unrecognized arguments" in stderr: ensure a bare "--" is not being passed directly to the generator. The wrappers already strip a standalone separator; both example commands above are accepted.
- Port already in use: free a port with PowerShell

PowerShell
$p = Get-NetTCPConnection -LocalPort 8010 -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty OwningProcess; if ($p) { Stop-Process -Id $p -Force }
$p = Get-NetTCPConnection -LocalPort 8020 -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty OwningProcess; if ($p) { Stop-Process -Id $p -Force }

- Where are artifacts saved? For these live runs, charts are not required; the Phase 2 example uses --no-artifacts to skip PNG writes entirely.
- Run location: execute commands from the repo root (C:\Users\ashis\Desktop\NeuroForge) so relative paths resolve.