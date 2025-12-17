_Updated: 2025-11-29 • Version: 44abe21e02554ecae199f3c316c57faf25da4290_

# Phase C Telemetry & MemoryDB

This document summarizes Phase C telemetry signals and the MemoryDB schema used for downstream analysis and validation.

## Live JSON Events
- `C:consolidation` — emitted once per consolidation cycle when the consolidation count increases.

Event payload fields:
- `count` — number of consolidation events since previous emission.
- `total` — cumulative consolidation event count since start.
- `rate` — instantaneous consolidation event rate (events/sec).
- `active_synapses` — active synapses at emission.
- `potentiated_synapses` — synapses marked potentiated.
- `depressed_synapses` — synapses marked depressed.

## MemoryDB Configuration
- `NF_TELEMETRY_DB` — path to SQLite DB for telemetry (e.g., `C:\path\to\telemetry.db`).
- `NF_ASSERT_ENGINE_DB` — when set, seeds initial rows and asserts telemetry presence in short runs.
- `NF_MEMDB_INTERVAL_MS` — periodic sampling interval in milliseconds.
- CLI override: `--memdb-interval=MS` supersedes `NF_MEMDB_INTERVAL_MS`.

Example PowerShell setup:
```powershell
$env:NF_TELEMETRY_DB = "C:\Users\ashis\Desktop\NeuroForge\phase_c_run_learn.db"
$env:NF_ASSERT_ENGINE_DB = "1"
$env:NF_MEMDB_INTERVAL_MS = "50"
```

## MemoryDB Tables (key schemas)

### experiences
- `id` INTEGER PRIMARY KEY
- `run_id` INTEGER
- `ts_ms` INTEGER
- `step` INTEGER
- `tag` TEXT
- `input_json` TEXT
- `output_json` TEXT
- `significant` INTEGER (0/1)

Example `triplet_ingestion` payload:
```json
{
  "image": "00123.jpg",
  "audio": "00123.wav",
  "caption": "A woman sitting on a bench.",
  "tokens": ["A","woman","sitting","on","a","bench."],
  "teacher_id": "triplet_00123"
}
```

Notes:
- Emitted when a new triplet is loaded into the unified loop.
- Linked to current episode via `episode_experiences` for trajectory reconstruction.
- `step` uses the canonical brain timeline (`processing_cycles`).
- Correlate with `reward_log` rows using `step` and `teacher_id`.

### actions (New: Sandbox Interaction Logging)
- `id` INTEGER PRIMARY KEY
- `run_id` INTEGER
- `ts_ms` INTEGER
- `step` INTEGER
- `type` TEXT (e.g., `cursor_move`, `scroll`, `click`, `type_text`, `key_press`)
- `payload_json` TEXT (action parameters, e.g., coordinates, direction)
- `success` INTEGER (0/1)

Notes:
- Notes:
- Emitted when the agent performs an action in the sandbox or OS.
- Aligns with `processing_cycles` for `step` to correlate with rewards and learning.
- Typical payloads:
  - `{"target_x":...,"target_y":...,"grid":...,"best_index":...}` for `cursor_move`
  - `{"cx":...,"cy":...,"grid":...,"best_index":...}` for `click`
  - `{"dir":"up|down"}` for `scroll`
  - `{"text":"..."}` for `type_text`

#### Action Gating Telemetry (New)
- Unified gating attaches explicit reasons to each action.
- `payload_json.filter` contains:
  - `allow`: boolean
  - `reason`: `ok | no_web_actions | phase15_deny | phase13_freeze`
  - `phase`: `{ blocked_by_phase15, blocked_by_phase13, blocked_by_no_web_actions, blocked_by_simulate_flag }` (integers)
- Sample `payload_json`:
```json
{
  "cx": 512, "cy": 384,
  "grid": "32x18",
  "best_index": 287,
  "filter": {
    "allow": false,
    "reason": "phase15_deny",
    "phase": {
      "blocked_by_phase15": 1,
      "blocked_by_phase13": 0,
      "blocked_by_no_web_actions": 0,
      "blocked_by_simulate_flag": 0
    }
  }
}
```

### Vision meta & foveation (New)
- Live JSON logs include a `vision` meta block with the current capture rectangle and foveation configuration.
- Example:
```json
{
  "vision": {
    "source": "screen",
    "retina": {"x": 320, "y": 180, "w": 640, "h": 360},
    "foveation": {"enabled": true, "mode": "attention", "alpha": 0.3}
  }
}
```
- Fields:
  - `retina`: active capture rectangle in screen coordinates.
  - `foveation.enabled`: whether dynamic focusing is active.
  - `foveation.mode`: `cursor|center|attention`.
  - `foveation.alpha`: EMA smoothing factor for the fovea center.
- Implementation reference: `src/main.cpp:7305` (retina rect export) and `src/main.cpp:7311` (foveation config export).

### reward_log
- `id` INTEGER PRIMARY KEY
- `run_id` INTEGER
- `ts_ms` INTEGER
- `step` INTEGER
- `reward` REAL
- `source` TEXT
- `context_json` TEXT

Notes:
- `step` uses the canonical brain timeline (`processing_cycles`), aligned with `experiences` and `substrate_states`.
- `source` values include `phase_a`, `phase_c_survival`, `shaped`, and `merged`.
- `context_json` carries episode metadata and Phase-specific fields.
  - Phase A fields: `modality`, `teacher_id`, `similarity`, `novelty`, `total_reward`, `shaped`, `success`.

#### Blocked-Action Metrics (New)
- When gating occurs, `context_json.blocked_actions` includes counters:
  - `blocked_action_count`
  - `blocked_by_phase15`
  - `blocked_by_phase13`
  - `blocked_by_no_web_actions`
  - `blocked_by_simulate_flag`
- Synthetic triggers: `--simulate-blocked-actions=N` increments these counters per step without gating real actions.

### autonomy_envelope_log (Phase 13 / Stage 6.5)
- `id` INTEGER PRIMARY KEY
- `run_id` INTEGER
- `ts_ms` INTEGER
- `decision` TEXT
- `driver_json` TEXT

Notes:
- Populated by Phase 13 and the Stage 6.5 `AutonomyEnvelope` helper.
- Stage 6.5 computations use `decision="compute"` and write a structured JSON payload into `driver_json`.
- The JSON payload contains:
  - `autonomy_score` in `[0,1]`
  - `tier`: `NONE|SHADOW|CONDITIONAL|FULL`
  - Component contributions: `self_component`, `ethics_component`, `social_component`
  - Permission flags: `allow_action`, `allow_goal_commit`, `allow_self_revision`
  - `ts_ms` and `step` for alignment with other telemetry
  - `inputs`: clamped input features (`identity_confidence`, `self_trust`, `ethics_score`, `ethics_hard_block`, `social_alignment`, `reputation`)
  - `rationale`: human-readable explanation string
  - Optional `context` label (e.g., `"sandbox_action"`)
- Typical CSV export: `autonomy_envelope_log_run_<id>.csv` with columns `id,run_id,ts_ms,decision,driver_json`.

### autonomy_modulation_log (Phase 6 / Stage 7)
- `id` INTEGER PRIMARY KEY
- `run_id` INTEGER
- `ts_ms` INTEGER
- `autonomy_score` REAL
- `autonomy_tier` TEXT
- `autonomy_gain` REAL
- `ethics_hard_block` INTEGER
- `ethics_soft_risk` REAL
- `pre_rank_entropy` REAL
- `post_rank_entropy` REAL
- `exploration_bias` REAL
- `options_considered` INTEGER
- `option_rank_shift_mean` REAL
- `option_rank_shift_max` REAL
- `selected_option_id` INTEGER
- `decision_confidence` REAL
- `autonomy_applied` INTEGER
- `veto_reason` TEXT

Notes:
- Populated by `Phase6Reasoner::scoreOptions` when Stage 7 autonomy modulation is active.
- Captures how the read-only autonomy envelope and self-trust state perturb option rankings without granting direct control.
- `pre_rank_entropy`/`post_rank_entropy` measure the sharpness of option distributions before/after modulation.
- `exploration_bias` encodes how strongly rankings are nudged toward the mean to encourage exploration.
- `option_rank_shift_mean`/`option_rank_shift_max` summarize how far individual options moved in rank.
- `autonomy_applied` is `1` when modulation changed scores; `0` when autonomy had no effect.
- `veto_reason` explains why modulation was not applied (for example `ethics_hard_block` or `no_autonomy_influence`).

Analysis helpers:
- `scripts/analyze_autonomy_envelope.py` performs lightweight autonomy envelope and ethics sanity checks over a MemoryDB (row counts, sampled tiers, ethics veto dominance).

### learning_stats
- `id` INTEGER PRIMARY KEY
- `ts_ms` INTEGER
- `step` INTEGER
- `processing_hz` REAL
- `total_updates` INTEGER
- `hebbian_updates` INTEGER
- `stdp_updates` INTEGER
- `reward_updates` INTEGER
- `avg_weight_change` REAL
- `consolidation_rate` REAL
- `active_synapses` INTEGER
- `potentiated_synapses` INTEGER
- `depressed_synapses` INTEGER
- `avg_energy` REAL (New: Average metabolic energy across all neurons 0.0-1.0)
- `metabolic_hazard` REAL (New: Aggregate metabolic stress indicator)

Triplets ingestion correlation:
- Expect positive correlation between `reward_updates` and mimicry success during Phase A.
- Learning stats `step` uses the canonical timeline (`processing_cycles`) for perfect alignment with rewards and experiences.
- Use `scripts/phase_c_compute_correlations.py` to quantify alignment between rewards and learning metrics.

### self_model
- `id` INTEGER PRIMARY KEY
- `run_id` INTEGER
- `ts_ms` INTEGER
- `step` INTEGER
- `state_json` TEXT
- `confidence` REAL

### substrate_states
- `id` INTEGER PRIMARY KEY
- `run_id` INTEGER
- `ts_ms` INTEGER
- `step` INTEGER
- `state_type` TEXT (e.g., `synapse_weights`)
- `region_id` TEXT or INTEGER
- `serialized_data` TEXT (JSON payload; keys depend on `state_type`)

### hippocampal_snapshots
- `id` INTEGER PRIMARY KEY
- `run_id` INTEGER
- `ts_ms` INTEGER
- `step` INTEGER
- `priority` REAL
- `significance` REAL
- `snapshot_data` TEXT (JSON payload)

### episodes / episode_experiences / episode_stats
- Episodes are linked groupings of experiences with contextual metadata.
- Join table associates episodes ↔ experiences; stats include aggregates per episode.
- Use `scripts/db_inspect.py` to view exact column names in your build.

## Phase 7 MemoryDB Tables

### affective_state
- `id` INTEGER PRIMARY KEY
- `run_id` INTEGER
- `ts_ms` INTEGER
- `episode_index` INTEGER
- `state_json` TEXT (valence/arousal and related affect features)
- `confidence` REAL (optional; build-dependent)

### reflections
- `id` INTEGER PRIMARY KEY
- `run_id` INTEGER
- `ts_ms` INTEGER
- `episode_index` INTEGER
- `payload_json` TEXT or `text` (build-dependent; reflection summary/metadata)

Note: schemas can vary slightly by build; use `scripts/db_inspect.py` to confirm exact columns.

## Analysis & Validation
- Plots and summaries: `scripts/phase_c_analyze_run.py` on CSV logs.
- Correlations and lag analysis: `scripts/phase_c_compute_correlations.py` on the telemetry DB.
- DB inspection: `scripts/db_inspect.py` for schema and sample rows.

Notes:
- Some metrics (e.g., `consolidation_rate`) may be constant/zero depending on run configuration, resulting in `NaN` correlations.
- JSON fields are large; use `--limit-json` in `db_inspect.py` to print only column info plus a single sample.

## Reward Interval Decoupling (Phase A)
- Control mimicry reward logging cadence independently of general telemetry.
- CLI: `--reward-interval=MS`; Env fallback: `NF_REWARD_INTERVAL_MS`.
- Typical setup aligns `NF_MEMDB_INTERVAL_MS=500` with `NF_REWARD_INTERVAL_MS=60000` for sparse reward logging.
