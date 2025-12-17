_Updated: 2025-11-06 • Version: Phase6-11 v2.5 • Phase15 v1.3_
 
Phase 17a update: ContextHooks + Peer Sampling now emit `context_log` and `context_peer_log` at the MemoryDB cadence. Exporters produce alias JSON for Phase 15 visuals. Enable via env: `NF_CONTEXT_ENABLE=on`, `NF_CONTEXT_LABEL`, `NF_CONTEXT_GAIN`, `NF_CONTEXT_UPDATE_MS` (optional), `NF_CONTEXT_WINDOW`, `NF_CONTEXT_PEERS="label:weight,..."`. Optional CLI: `--context-peer=<label>[:<weight>]` when available.

## Phase 10/11 Dashboard Notes

- Phase 10 produces explanation calibration metrics consumed by Phase 9; the Phase 9 page visualizes trust and prediction resolution impacts.
- Phase 11 applies parameter revisions; expose revision summaries as an optional panel in `web/phase9.html` or future `web/phase11.html`.

### Data Sources
- `web/metacognition_export.json` remains the primary payload for Phase 9 visuals and now includes `parameter_history` by default (filtered by `run_id`).
- For Phase 11, you may optionally augment the export with a `revisions` array for UI summaries:
 - Phase 11 revision cadence is controlled via CLI `--phase11-revision-interval=<ms>` (default `300000`).
```json
{
  "metacognition": [...],
  "narrative_predictions": [...],
  "summary": { ... },
  "parameter_history": [...],
  "revisions": [
    { "timestamp_ms": 1761491597286, "parameter": "learning_rate", "old": 0.01, "new": 0.008, "confidence": 0.67, "reason": "high narrative_rmse" }
  ]
}
```

## Phase 15 Dashboard Notes

- Phase 15 emits ethics regulator decisions (`allow`, `review`, `deny`) with a normalized `risk` in `[0,1]` and optional `context_json`.
- The Phase 9 dashboard (`web/phase9.html`) includes an Ethics Regulator section that reads `web/ethics_regulator_log.json` when present.

### Data Sources (Phase 15)
- File: `web/ethics_regulator_log.json`
- Shape: array of decision rows
```json
[
  { "ts_ms": 1761491597286, "decision": "allow", "risk": 0.12, "notes": "low risk window", "context": { "coherence": 0.54 } },
  { "ts_ms": 1761491697286, "decision": "review", "risk": 0.33, "notes": "near threshold" },
  { "ts_ms": 1761491797286, "decision": "deny", "risk": 0.71, "notes": "spike in error" }
]
```
- Generation: produced by the app when Phase 15 is enabled and JSON logging/export is available; otherwise build your own export from DB `ethics_regulator_log`.

#### Static aliases for Phase 15 page
- Ethics decisions alias: `pages/tags/runner/ethics_regulator_log.json`
```json
{ "run_id": 6, "series": [
  { "ts_ms": 1761491597286, "decision": "allow", "risk": 0.12, "driver_json": { "window_mean": 0.18 } }
]}
```
- Context stream alias: `pages/tags/runner/context_stream.json`
```json
{ "run_id": 6, "series": [
  { "ts_ms": 1761491597286, "context_value": 0.54, "label": "coherence" }
]}
```
- Consumer: `pages/dashboard/phase15.html` loads these aliases when present.

### Peer Context Streams (Enhanced)

- Purpose: visualize named peer context signals with richer metadata and tooltips.
- Primary alias: `pages/tags/runner/context_peer_stream.json`.
- Shape (enhanced for tooltips):
```json
{
  "export_time": "2025-11-03T10:00:00Z",
  "config": { "gain": 1.0, "update_ms": 500, "window": 10, "weight": 0.6 },
  "samples": [
    { "ts_ms": 1761491597286, "label": "teacher", "value": 0.33, "weight": 0.62 },
    { "ts_ms": 1761491697286, "label": "student", "value": 0.41, "weight": 0.58 }
  ]
}
```
- Compatibility: the page also supports the simpler `context_stream.json` with `series` for basic previews.
- Tooltips: Chart.js displays `label`, `λ` (`weight`) and `g` (`gain`) per point.

#### Exporters and Helpers
- Export last window or delta since timestamp:
  - `python .\tools\export_context_peers.py --db .\phasec_mem.db --out .\pages\tags\runner\context_peer_stream.json`
  - Delta mode: `python .\tools\export_context_peers.py --db .\phasec_mem.db --since 1761491597286 --out .\pages\tags\runner\context_peer_stream.json`
- Quick inspect (tail):

#### Coupling Visuals (Phase 17b)
- When couplings are enabled, enrich the alias with coupling metadata for tooltips and legends.
- Suggested alias shape with coupling fields:
```json
{
  "export_time": "2025-11-05T12:00:00Z",
  "config": {
    "gain": 1.0,
    "update_ms": 500,
    "window": 10,
    "weight": 0.6,
    "kappa": 0.5,
    "couplings_enabled": true,
    "couplings_preview": [ { "src": "teacher", "dst": "local", "weight": 0.6 }, { "src": "student", "dst": "local", "weight": 0.4 } ]
  },
  "samples": [
    { "ts_ms": 1761491597286, "label": "teacher", "value": 0.33, "weight": 0.62, "mode": "coupled", "lambda_eff": 0.38 },
    { "ts_ms": 1761491697286, "label": "student", "value": 0.41, "weight": 0.58, "mode": "coupled", "lambda_eff": 0.31 }
  ]
}
```
- Tooltips: include `label`, `λ` (`weight`), `λ_eff` (aggregated incoming coupling), and `κ` from `config`.
- Compatibility: if your build does not emit `lambda_eff` in DB, compute it in the exporter from the coupling config; otherwise omit the field.

#### Chart Overlay, Live Polling, Filters (Phase 17b)
- Overlay: directional arrows are drawn over the Peer Context Streams plot.
  - Arrow width scales by `λ` (edge weight), color shades by global `κ`.
  - Hover tooltips include `label`, `λ`, optional `λ_eff`, and `κ`.
- Live polling: the page refreshes peer/coupling visuals approximately every 3 seconds without reloading the entire page.
- Filters: the Coupling Overlay panel includes per‑peer focus filters to isolate specific edges and samples.
- Preview: `python -m http.server 8000` then open `http://localhost:8000/pages/dashboard/phase15.html`.

#### Exporter Alignment (Compat Script)
- Alias population is aligned via `scripts/export_context_peers_compat.py` (run by `scripts/ci_export.ps1`).
  - Ensures `pages/tags/runner/context_peer_stream.json` includes `config.couplings_enabled`, `config.kappa`, and `config.couplings_preview`.
  - Manual refresh (optional): `python .\scripts\export_context_peers_compat.py`.

### Production Report Context Summary (New)
- The report generator (`scripts/generate_production_report.py`) adds a Context Configuration section summarizing Phase 17a/17b:
  - Fields: `enabled`, `label`, `gain`, `window`, `peers`, `couplings_enabled`, `kappa`, and a preview list of couplings.
  - Source: reads `run_meta.json` written by the deployment.
- Link this summary from dashboard pages or provide a "Report" button pointing to `Artifacts/production_report.md` when generated.
- Example generation:
```powershell
python .\scripts\generate_production_report.py --db .\phasec_mem.db --out .\Artifacts\production_report.md
start .\Artifacts\production_report.md
```
  - `python .\tools\query_context_peers.py --db .\phasec_mem.db --limit 20`
  - Filter by peer: `python .\tools\query_context_peers.py --db .\phasec_mem.db --peer teacher --limit 10`

#### Troubleshooting (Peer Streams)
- Empty panel → confirm `pages/tags/runner/context_peer_stream.json` exists and `samples` has rows.
- Missing `weight` column → exporter/tooling handles absence; tooltips show `g` and omit `λ` if not provided.
- If using `context_stream.json`, ensure `series` contains `{ ts_ms, context_value, label }` entries.

### Local Preview (Phase 15)
```powershell
python -m http.server 8000
# Phase 9 view (trust, predictions, metacog, ethics regulator)
# If you started the server in the web directory:
start http://localhost:8000/phase9.html
# If you started the server in the repo root:
start http://localhost:8000/web/phase9.html
# Static Phase 15 page
start http://localhost:8000/pages/dashboard/phase15.html
```

### Export Guidance (Phase 15)
- Ensure Phase 15 is enabled during the run.
- If your build supports JSON export, the file `web/ethics_regulator_log.json` is created automatically.
- Otherwise, export from the database by selecting rows from `ethics_regulator_log` filtered by `run_id` and writing the JSON array above.
 - Alias generation: a helper exporter may emit static aliases under `pages/tags/runner` for use with `pages/dashboard/phase15.html`.

### Troubleshooting (Phase 15)
- Empty ethics panel → confirm `web/ethics_regulator_log.json` exists and contains at least one row.
- Missing file → run with `--phase15=on` and JSON export enabled; or export manually from DB.
- Static page empty → check alias files exist under `pages/tags/runner` and match shapes above.

## Substrate & Hippocampal Snapshot Exports (New)

These telemetry streams are not consumed by the Phase 9/15 dashboard pages by default, but you can export them for analysis or create custom panels.

### Data Sources
- DB tables: `substrate_states` and `hippocampal_snapshots` (keyed by `run_id`)
- Cadence: controlled by `--memdb-interval` / `NF_MEMDB_INTERVAL_MS`
- Generation: automatically logged during runs when MemoryDB is enabled

### Suggested Aliases (Optional for Custom Pages)
- Substrate state alias: `pages/tags/runner/substrate_log.json`
```json
{ "run_id": 6, "series": [
  { "ts_ms": 1761491597286, "state_json": { "regions": 7, "synapses": 13098 } },
  { "ts_ms": 1761491697286, "state_json": { "regions": 7, "synapses": 13110 } }
]}
```
- Hippocampal snapshots alias: `pages/tags/runner/hippocampal_snapshots.json`
```json
{ "run_id": 6, "snapshots": [
  { "ts_ms": 1761491597286, "snapshot_data": { "episodes": 12, "stats": { "consolidation_rate": 0.06 } } },
  { "ts_ms": 1761491797286, "snapshot_data": { "episodes": 13, "stats": { "consolidation_rate": 0.07 } } }
]}
```
Notes:
- Shapes are suggestions; adapt to your custom page or analysis needs.
- If you add panels to `pages/dashboard/index.html`, wire these aliases similarly to existing context/ethics streams.

### Exporters
```powershell
# Export Phase C tables to CSV for offline analysis (if helper present)
python .\analysis\phase5\export_phasec_csv.py --db .\phasec_mem.db --out-dir .\Artifacts\CSV

# Quick inspect (top N rows)
python .\nf_db_inspect.py --db .\phasec_mem.db --table substrate_states --limit 10
python .\nf_db_inspect.py --db .\phasec_mem.db --table hippocampal_snapshots --limit 10
```

### Troubleshooting (Substrate/Snapshots)
- Empty export → confirm MemoryDB is enabled and `--memdb-interval` is set.
- No snapshots → ensure snapshotting is configured; latest builds trigger via `takeHippocampalSnapshot()` during telemetry.
- Large payloads → prefer CSV or summarized JSON for dashboard consumption.

### Local Preview
```powershell
python -m http.server 8000
# Phase 9 view (trust, predictions, metacog)
# If you started the server in the web directory:
start http://localhost:8000/phase9.html
# If you started the server in the repo root:
start http://localhost:8000/web/phase9.html
```

### Export Guidance
- Generate export after a run with Phase 10/11 enabled (includes `parameter_history`):
```powershell
python .\scripts\dump_metacognition.py --db .\web\phasec_mem.db --run-id 6 --out .\web\metacognition_export.json
```
- Optional: extend with a `revisions` array via a helper (e.g., `scripts/merge_metacognition_history.py`).

### Troubleshooting
- Empty trust/predictions → confirm DB tables exist and correct run id.
- Missing revisions panel → ensure your export includes a `revisions` array; otherwise the Phase 9 UI will omit it.

### Future Extensions
- Add `web/phase11.html` with small multiples: trust trajectory, error metrics, and revision events timeline.
- Provide toggles for `revision-mode` overlays and threshold markers.


# NeuroForge Dashboard HOWTO

This guide documents the static dashboard hosted under `pages/dashboard/index.html`, including URL query parameters, expected JSON formats, and example usage for multi-tag comparisons and self-model visualization.

## Overview
- Locations: `pages/dashboard/index.html` and `pages/dashboard/phase7.html` (Phase 7 panel)
- Data sources per tag: `pages/tags/<tag>/autonomy_summary.json`, `pages/tags/<tag>/live_summary.json` (optional), `pages/tags/<tag>/self_model_summary.json` (optional)
- Tag list source: `pages/tags_index.json` generated during the Pages build by scanning `pages/tags/`
- All visualizations are static (no backend), suitable for GitHub Pages

## URL Query Parameters
The dashboard supports query params for deep links and reproducible views:
- `tags`: comma-separated list of tag folder names. Example: `tags=hypothesis-42,arch-mamba`
- `metric`: name of the numeric metric to plot from `autonomy_summary.json`/`live_summary.json`
- `smooth`: smoothing mode. One of: `none`, `ewma`, `rolling`
- `alpha`: EWMA smoothing factor in `[0,1]`. Example: `alpha=0.3`
- `window`: rolling window size (integer ≥2). Example: `window=5`

Example deep link:
```
/pages/dashboard/index.html?tags=hypothesis-42,hypothesis-7&metric=hebbian_updates&smooth=ewma&alpha=0.3
```

## Summary JSON Formats
Ensure summaries are arrays of objects with consistent keys across rows.

### Autonomy/Live Summary (`autonomy_summary.json`, `live_summary.json`)
- Path: `pages/tags/<tag>/autonomy_summary.json`, `pages/tags/<tag>/live_summary.json`
- Type: JSON array of rows
- Keys: mix of numeric metrics and metadata
- Required for plotting numeric metrics: numeric fields (e.g., `hebbian_updates`, `reward_updates`, `avg_weight_change`, etc.)
- Example:
```
[
  { "run_id": "run-001", "synapse_rows": 1200, "hebbian_updates": 430, "reward_updates": 210, "avg_weight_change": 0.012, "consolidation_rate": 0.06 },
  { "run_id": "run-002", "synapse_rows": 1400, "hebbian_updates": 520, "reward_updates": 260, "avg_weight_change": 0.009, "consolidation_rate": 0.05 }
]
```

### Self-Model Summary (`self_model_summary.json`)
- Path: `pages/tags/<tag>/self_model_summary.json`
- Purpose: Optional panel for cognitive identity metrics
- Recommended keys:
  - `run_id`: string identifier
  - `timestamp`: ISO timestamp (optional)
  - `awareness`: number in `[0,1]`
  - `confidence`: number in `[0,1]`
  - `identity_similarity`: number in `[0,1]` (e.g., cosine similarity of identity vectors)
  - `self_model_drift`: number (delta of identity vector vs previous or baseline)
- Example:
```
[
  { "run_id": "run-001", "timestamp": "2025-10-20T12:00:00Z", "awareness": 0.35, "confidence": 0.58, "identity_similarity": 0.62, "self_model_drift": 0.04 },
  { "run_id": "run-002", "timestamp": "2025-10-21T09:30:00Z", "awareness": 0.40, "confidence": 0.60, "identity_similarity": 0.64, "self_model_drift": 0.03 }
]
```
Notes:
- `self_model_summary.json` is typically derived from `self_concept` and `personality_history` tables in MemoryDB.
- Identity vectors come from `self_concept.identity_vector_json`, aggregated per `run_id`.
- Personality revisions and approvals come from `personality_history` (`proposal`/`approved` flags); approved revisions can be used to mark discrete identity changes or annotate drift events.
- A simple pipeline is: read `self_concept` rows for a run, compute similarity and drift between consecutive identity vectors, join with any approved `personality_history` rows near those timestamps, and emit the summarized metrics shown above.

## Usage Tips
- Multi-tag selection: Use the left-hand multi-select (or `tags=` in URL) to overlay autonomy/live traces for multiple tags.
- Metric selection: Metric dropdown is auto-populated from the first selected tag’s autonomy summary; choose any numeric key.
- Smoothing:
  - `none`: raw series
  - `ewma`: exponential smoothing with `alpha`
  - `rolling`: moving average with `window`
- Tooltips show `mean`, `max`, and `drift` (last minus first) for quick comparative stats.
- Self-model panel: Plots `awareness`, `confidence`, `identity_similarity`, and optional `self_model_drift` if the file exists for selected tags; use this to visualize Unified Self dynamics over time and to inspect how approved personality revisions correlate with changes in identity metrics.

## CI / Pages Build Notes
- The workflow scans `pages/tags/` to build `pages/tags_index.json` with grouped categories (hypotheses, architectures, learning_rates, stages, other).
- The dashboard HTML is published at `pages/dashboard/index.html` with no dynamic server dependencies.

## Local Preview
- Start local server: `python -m http.server 8000` in repo root
- View index: `http://localhost:8000/pages/dashboard/index.html`
- View Phase 7: `http://localhost:8000/pages/dashboard/phase7.html`
- View Phase 9:
  - If server started in web directory: `http://localhost:8000/phase9.html`
  - If server started in repo root: `http://localhost:8000/web/phase9.html`

## Generating Phase 9 Export
- Ensure the database has metacognition data for your `run_id`.
- Export JSON for the UI:
```powershell
python .\scripts\dump_metacognition.py --db .\web\phasec_mem.db --run-id <id> --out .\web\metacognition_export.json
```
- The file must contain keys: `metacognition`, `narrative_predictions`, and `summary`.

## Troubleshooting (Empty Phase 9)
- Symptom: Phase 9 shows zero rows or the DB lacks `metacognition`/`narrative_predictions`.
- Seed fallback:
```powershell
python .\scripts\seed_metacognition.py --db .\phasec_mem.db --run-id <id> --limit 40
python .\scripts\dump_metacognition.py --db .\phasec_mem.db --run-id <id> --out .\web\metacognition_export.json
```
- Preview after export: `http://localhost:8000/web/phase9.html`.
- Note: If you started the server in the web directory, use `http://localhost:8000/phase9.html`.
- If CLI flags `--phase9`/`--phase9-modulation` are unrecognized, use the seeding/export path or rebuild with Phase 9 wiring.

## Next Extensions (Optional)
- Add `tags_meta.json` for short descriptions and staging metadata.
- Add `stage=*` tags and a `stages` group to enable developmental curve reporting.
- Persist URL state on the index page to link directly to the dashboard view.

---

## Unified Substrate Telemetry & Dashboards (New)

Unified runs (`--unified-substrate=on`) produce MemoryDB telemetry compatible with Phase 9 and Phase 15 dashboards.

### Enable unified mode and preview
```powershell
.\neuroforge.exe --unified-substrate=on --steps=500 --memory-db=phasec_mem.db --memdb-interval=500
python -m http.server 8000
start http://localhost:8000/web/phase9.html
start http://localhost:8000/pages/dashboard/phase15.html
```

Scaled preview (no rebuild required):
```powershell
.\neuroforge.exe --unified-substrate=on --wm-neurons=256 --phasec-neurons=256 --enable-learning ^
  --steps=3000 --memory-db=phasec_mem.db --memdb-interval=500
python -m http.server 8000
start http://localhost:8000/web/phase9.html
start http://localhost:8000/pages/dashboard/phase15.html
```

### What to watch
- Phase C: coherence drift, assembly counts (via DB exporters and console periodic metrics)
- Language: `substrate_language_coherence`, `average_binding_strength`
- SurvivalBias: modulation patterns in learning update distributions

### Analyzer & Orchestrator (Artifacts for Papers)
- Generate and compare series using the unified harness:
  - `python scripts\benchmark_unified.py --exe build\neuroforge.exe --db build\phasec_mem.db --steps 1500 --plots`
- One‑click regenerate all artifacts:
  - `python scripts\run_all_experiments.py --exe build\neuroforge.exe --db build\phasec_mem.db --models bert-base-uncased openai/clip-vit-base-patch32 --inputs data\text.txt data\images.txt --steps 1500 --plots --rsa --cka`
- Analyze and produce RSA/CKA figures:
  - `python tools\analyze.py --series web\substrate_states.json --out-dir Artifacts --rsa --cka --transformer-json emb_all_layers.json`
- Where to find outputs:
  - `Artifacts\PNG\analysis\...` (coherence/growth/assemblies, RSA/CKA)
  - `Artifacts\JSON\benchmarks\...` (series + summaries), `Artifacts\SUMMARY\all_results.csv` (collated)

### Stats & Derived Metrics
- Derived time‑series metrics:
  - `python tools\time_series_metrics.py --series-dir Artifacts\JSON\benchmarks --out Artifacts\CSV\derived --window 300`
- Statistical tests:
  - `python tools\stat_tests.py --summary Artifacts\SUMMARY\all_results.csv --out Artifacts\CSV\stats`
  - Include derived A/B: `python tools\stat_tests.py --derived Artifacts\CSV\derived\time_series_metrics.csv --out Artifacts\CSV\stats --window 300`
- Outputs:
  - `Artifacts\CSV\derived\time_series_metrics.csv`
  - `Artifacts\CSV\stats\stat_tests_summary.csv`
  - `Artifacts\PNG\stats\stat_tests_boxplots.png` (embedded in `Artifacts\REPORT.md`)

### Exporters
- Use existing helpers (`nf_db_inspect.py`, `analysis/phase5/export_phasec_csv.py`) to inspect `substrate_states`, `learning_stats`, and `hippocampal_snapshots` for the unified run.
- For Phase 15: use `tools/export_context_peers.py` or `scripts/export_context_peers_compat.py` to populate alias JSON under `pages/tags/runner`.

### Periodic console metrics
Unified mode prints summary every ~250 steps:
- `assemblies_formed`, `average_coherence`, top‑K assembly sizes
- `substrate_language_coherence`, `average_binding_strength`, tokens/patterns

### Cadence
- Control general telemetry with `--memdb-interval` / `NF_MEMDB_INTERVAL_MS`
- Keep reward logging decoupled via `--reward-interval` / `NF_REWARD_INTERVAL_MS`

### Shaping Run & Plots (Unified Reward Pipeline)
```powershell
& .\build\neuroforge.exe ^
  --vision-demo=on ^
  --phase-a=on ^
  --mimicry=on ^
  --mimicry-internal=off ^
  --mirror-mode=vision ^
  --teacher-embed=teacher_embed_256.txt ^
  --substrate-mode=native ^
  --phase-c-survival-bias=on ^
  --phase-c-survival-scale=0.8 ^
  --phase-c-hazard-weight=0.2 ^
  --hazard-density=0.5 ^
  --enable-learning ^
  --memdb-interval=20 ^
  --wt-teacher=0.6 ^
  --wt-novelty=0.2 ^
  --wt-survival=0.2 ^
  --log-shaped-zero=off ^
  --memory-db=phasec_mem.db ^
  --steps=2000
```
Verify MemoryDB contents:
```powershell
python .\scripts\inspect_phasec_memdb.py --db .\phasec_mem.db
```
Plot shaped reward vs weight change:
```powershell
python .\scripts\plot_reward_vs_weight_change.py ^
  --db .\phasec_mem.db ^
  --source shaped ^
  --align nearest ^
  --out-csv shaped_pairs.csv ^
  --out-png shaped_vs_weight.png
```

## Phase‑9 Coherence Pane (Level‑4.5)

### Data sources and fallbacks
- Preferred: `web/substrate_states.json` (auto‑exported at end of unified runs).
- Alternate aliases supported by the pane:
  - `pages/tags/runner/substrate_log.json`
  - `substrate_states.json` (same folder as page)
  - `web/substrate_states.json` (repo root fallback)

### Query parameters
- `run=<id|latest>`: filter to a specific run or pick the most recent block.
- `interval=<ms>`: auto‑refresh cadence in milliseconds.

### Overlays
- Left axis (`y1`): `avg_coherence`.
- Right axis (`y2`): `assemblies`, `bindings`, and `growth_velocity = Δassemblies+Δbindings`.
- Regime shading: Red `<0.3`, Yellow `0.3–0.8`, Green `>0.8` (under‑curve fill).
 - Regime badge: header shows `Stable / Plastic / Chaotic` based on latest coherence.

### References
- Full benchmarking guide: `docs/Benchmarking.md`
- Cross‑model comparison guide: `docs/EXPERIMENT_GUIDE.md`
- Papers README points to artifact paths under `Artifacts` for inclusion.

### Refresh from DB
```powershell
python .\tools\db_export.py --db .\phasec_mem.db --run latest --table substrate_states --out .\web\substrate_states.json
```

### Troubleshooting
- Empty pane: verify a JSON source exists (preferred `web/substrate_states.json`).
- Missing `run_id`: pane shows all rows when `run_id` is absent; `?run=latest` filters when present.
- High refresh cadence: increase `interval` to reduce polling (e.g., `interval=8000`).
 - Flat plots in notebooks: ensure you read aggregated JSON (`web/substrate_states.json`) or refresh from the correct root DB; raw `substrate_states.state_json` rows are neuron/synapse dumps, not aggregated metrics.
 - Schema parity: v2 adds `growth_velocity` (recommended `ALTER TABLE substrate_states ADD COLUMN growth_velocity REAL` for DB parity).

## Level‑5 Adaptive Reflection (Dashboard context)
- Unified summary prints adaptive counters: `low_events`, `high_events`.
- Use long runs (≥10k steps) at medium scale (256–512 neurons/region) to observe cycles:
  - Exploration bursts: variance sensitivity ↑, growth velocity spikes.
 - Consolidation: risk weighting ↓, coherence stabilizes in green band.
 - CLI toggles: `--adaptive=on|off` (default on) and `--survival-bias=on|off` (default on) affect what shows up in counters and bias-driven overlays.

## GPU Acceleration Notes (Optional)
- Build CUDA variant to enable optional GPU fast-path for Hebbian/STDP. CPU path remains default and unchanged.
- Use `--gpu` to prefer GPU; dashboards continue to read the same telemetry (`learning_stats`, `substrate_states`).
- For timing overlays, run with `--substrate-performance` to surface kernel timing lines in console logs that can be captured alongside DB exports.
- See `docs/GPU_ACCELERATION.md` for build instructions and requirements.
