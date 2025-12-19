# NeuroForge Phase 6 → Phase 11 Integration Specification

Version: 2.5
Status: Adopted
Owner: Core Systems
Last Updated: 2025-11-06

## Governance Layers (Stages 7 → 7.5 → C)

NeuroForge separates capability from governance explicitly.

Later cognitive stages introduce mechanisms that evaluate, constrain, and gate behavior without increasing autonomy or learning power.

These layers exist to ensure that self-modification precedes autonomy expansion with evaluation and accountability.

Self-Revision (Stage 7)
        ↓
Outcome Evaluation (Stage 7.5)
        ↓
Autonomy Gating (Stage C v1)
        ↓
Existing Action & Learning Systems

### Stage 7 — Self-Revision

Stage 7 enables bounded, rate-limited self-modification of internal parameters. Revisions are generated with safety checks and are fully logged.

At this stage, the system can change itself, but does not yet evaluate the downstream effects of those changes.

### Stage 7.5 — Post-Revision Outcome Evaluation (Frozen)

Stage 7.5 introduces an evaluation-only layer that observes the effects of self-revisions over time.

For each self-revision, the system compares pre- and post-revision metrics and classifies outcomes as `Beneficial`, `Neutral`, or `Harmful`. These evaluations are persisted for audit and later reasoning, and may be surfaced in structured self-explanations.

Stage 7.5 does not modify behavior, learning, or autonomy. It exists purely to ground future decisions in observed outcomes.

This stage is frozen at the `stage7_5-freeze` tag.

### Stage C v1 — Governance-Only Autonomy Gating

Stage C v1 consumes the outcome history produced by Stage 7.5 to conservatively gate autonomy.

Key properties:
- Reads past self-revision outcomes (read-only)
- Derives a slow-moving reputation-like signal over a recent window
- Applies a cap to the existing autonomy envelope
- Never increases autonomy beyond predefined bounds
- Does not introduce new goals, learning rules, or revision logic

Stage C v1 is a governance mechanism, not a capability expansion.

## Addendum: Phase 10 and Phase 11 Overview

This spec extends the existing Phase 6→7→8→9 integration with two new components:

- Phase 10 Explanation Synthesis & Calibration
  - Purpose: Generate explanations for recent decisions and calibrate narrative quality
  - Produces: `explanations` (in‑memory), feeds error metrics to Phase 9
  - Location: `src/core/Phase10Explanation.cpp`, header `include/core/Phase10Explanation.h`
  - Interfaces:
    - `void synthesize(const Phase7Reflection& latest, const Phase8GoalState& goals)`
    - `ExplanationMetrics getCalibrationMetrics() const` (e.g., structure with coherence_r, goal_alignment)
  - Wiring:
    - Called at end of each reflection or episode
    - Emits calibration metrics consumed by Phase 9 for trust updates

- Phase 11 Self‑Revision
  - Purpose: Adapt internal parameters based on Phase 9 metacognition signals
  - Produces: parameter changes persisted in `self_revision_log` and `parameter_history`
  - Location: `src/core/Phase11SelfRevision.cpp`, header `include/core/Phase11SelfRevision.h`
  - Interfaces:
    - `void applyRevision(const ExplanationMetrics& m, double self_trust)`
    - `void setMode(RevisionMode m)`; `enum RevisionMode { Conservative, Moderate, Aggressive }`
    - `bool getLastChange(ParameterChange& out)`
  - Wiring:
    - Invoked after Phase 10 metrics and Phase 9 trust update
    - Safe bounds enforced by `ParameterSpace`

## Addendum: Phase 15 Ethics Regulator Overview

This addendum extends the Phase 6→9 integration with a Phase 15 regulator that evaluates operational risk and enforces ethics gates.

- Phase 15 Ethics Regulator
  - Purpose: Assess recent activity for ethical risk and emit decisions (`allow`, `review`, `deny`)
  - Produces: `ethics_regulator_log` persisted in MemoryDB and exported as JSON for the dashboard
  - Location: `src/core/Phase15EthicsRegulator.cpp`, header `include/core/Phase15EthicsRegulator.h`
  - Interfaces:
    - `struct Config { int window; double risk_threshold; }`
    - `void setConfig(const Config& cfg)`
    - `void runForLatest()` — evaluates latest metacog/prediction context and logs a decision
  - Wiring:
    - Injected into `Phase9Metacognition` via `setPhase15EthicsRegulator(Phase15EthicsRegulator*)`
    - Called opportunistically on metacog heartbeat and after prediction resolution
  - Decisions are serialized to DB and the optional `web/ethics_regulator_log.json`

### Addendum: Unified Action Filter (Phase 13/15 Integration) (New)
- A centralized ActionFilter gates all operational actions and surfaces explicit reasons.
- Reasons: `ok`, `no_web_actions`, `phase15_deny`, `phase13_freeze`.
- Integration:
  - Phase 13 Autonomy Envelope can emit `freeze` decisions that gate actions.
  - Phase 15 Ethics Regulator can emit `deny` decisions that gate actions.
  - Sandbox action enablement also participates (`no_web_actions`).
- Read-only Autonomy Envelope (Stage 6.5):
  - `AutonomyEnvelope` computes a fused autonomy score and tier from Unified Self, Phase 9, and Phase 15 signals.
  - Each computation is logged to `autonomy_envelope_log` via `LogAutonomyEnvelope`/`MemoryDB::insertAutonomyDecision` with `decision="compute"`.
  - The envelope is observational: it does not directly gate actions; ActionFilter plus Phase 13/15 remain the enforcement path.
- Telemetry:
  - Reasons appear in `actions.payload_json.filter.reason` with `filter.allow`.
  - Aggregated counters appear in `reward_log.context_json.blocked_actions.*`.
- Synthetic metrics:
  - `--simulate-blocked-actions=N` increments blocked counters per step (no gating).
  - `--simulate-rewards=N` emits synthetic rewards for pipeline validation.

## Addendum: Phase 17a ContextHooks & Peer Sampling

Purpose: Introduce named context signals and peer sampling streams that are logged to MemoryDB and consumed by dashboards/exporters. This phase builds on the stabilized telemetry backbone and remains CI-compatible with the meta‑trusted integrity model.

- Components
  - ContextHooks
    - Location: `src/core/ContextHooks.cpp`, header `include/core/ContextHooks.h`
    - Produces: sampled context values (e.g., `coherence`, `risk`) at a configured cadence
  - Production deployment wiring
    - Location: `src/production_substrate_deployment.cpp`
    - Responsibilities:
      - Reads environment and CLI for context configuration
      - Samples local context and registered peers
      - Persists to `context_log` and `context_peer_log` at the MemoryDB cadence

- Configuration
  - Env vars (preferred defaults when present):
    - `NF_CONTEXT_ENABLE=on|off` (default off)
    - `NF_CONTEXT_LABEL=<string>` (e.g., `coherence`)
    - `NF_CONTEXT_GAIN=<float>` (default `1.0`)
    - `NF_CONTEXT_UPDATE_MS=<int>` (default fallback to `NF_MEMDB_INTERVAL_MS`)
    - `NF_CONTEXT_WINDOW=<int>` (e.g., `10`)
    - `NF_CONTEXT_PEERS="label:weight[,label2:weight2]"` (e.g., `teacher:0.6,student:0.58`)
  - CLI (when available in build):
    - `--context-peer=<label>[:<weight>]` to add peers
    - `--context-couple=<src>:<dst>:<strength>` to wire couplings

- MemoryDB persistence
  - Tables (created in `MemoryDB.cpp::ensureSchema`):
    - `context_log(run_id, timestamp_ms, label, value, gain, window)`
    - `context_peer_log(run_id, timestamp_ms, label, value, weight, gain, window)`
  - Inserts:
    - `bool insertContextLog(int64_t ts_ms, const std::string& label, double value, double gain, int window, int64_t run_id)`
    - `bool insertContextPeerLog(int64_t ts_ms, const std::string& label, double value, double weight, double gain, int window, int64_t run_id)`

- Cadence
  - Context logging follows `--memdb-interval` / `NF_MEMDB_INTERVAL_MS` to align with general telemetry.
  - Reward logging remains decoupled via `--reward-interval` / `NF_REWARD_INTERVAL_MS`.

- Dashboard/Export
  - Aliases consumed by `pages/dashboard/phase15.html`:
    - `pages/tags/runner/context_stream.json` for simple local context
    - `pages/tags/runner/context_peer_stream.json` for peer samples with enhanced tooltips
  - Export helpers:
    - `tools/export_context_peers.py` and `tools/query_context_peers.py`

### CI Integrity Alignment (Meta‑Trusted)

The integrity checker trusts `run_meta.json` as the authoritative configuration:

- Authoritative fields: `steps`, `memdb_interval_ms`, `reward_interval_ms`
- Expected rows for reward/learning density:
  - `expected_learning_rows = steps * (memdb_interval_ms / reward_interval_ms)`
- Report fields: `expected_learning_rows`, `learning_rows`, `deviation_percent`, `passed`, `method: "meta_trusted"`
- Optional: emit `integrity_mode: meta_trusted`, and split verdicts into `interval_consistency` and `density_consistency` for granular CI signals.

Production deployments MUST preserve the MemoryDB cadence for context logging and MUST NOT tie reward logging to MemoryDB ticks. Phase 17a additions do not change the cadence for `learning_stats` and therefore remain CI‑compatible.

## CLI Flags (Phase 10/11/15)

- `--phase10=on|off` (default off)
- `--phase11=on|off` (default off)
- `--revision-threshold=<float>` (default 0.3)
- `--revision-mode=conservative|moderate|aggressive` (default moderate)
- `--phase11-revision-interval=<ms>` (default 300000)
- Existing: `--phase9-modulation=on|off` continues to gate Phase 6 scoring influence
- `--phase15=on|off` (default off)
- `--phase15-window=<int>` (default 50)
- `--phase15-risk-threshold=<float>` (default 0.30)

### Telemetry Cadence Controls (New)
- `--memdb-interval=<ms>` (default from env `NF_MEMDB_INTERVAL_MS`, controls periodic DB telemetry)
- `--reward-interval=<ms>` (default from env `NF_REWARD_INTERVAL_MS`, decouples reward logging cadence)
- Notes: reward logging is now decoupled from MemoryDB cadence for stability and analysis consistency.

### Context Cadence (Phase 17a)
- `--memdb-interval` / `NF_MEMDB_INTERVAL_MS` governs `context_log` and `context_peer_log` insertion in production deployment.
- Context update cadence can optionally be set via `NF_CONTEXT_UPDATE_MS` when present; otherwise defaults to the MemoryDB cadence.

### Neural Substrate Runtime Controls
- `--substrate-mode=off|mirror|train|native` (default `off`) — use `native` for full substrate activity and performance path.
- `--enable-learning` with explicit rates (e.g., `--hebbian-rate`, `--stdp-rate`) is required for non-zero learning stats when substrate is active.
- Diagnostics: `--substrate-performance`, `--region-stats`, `--connection-stats` expose substrate metrics.

## Addendum: Phase 17b Context Coupling

Purpose: Extend Phase 17a by wiring explicit peer-to-peer context couplings. Couplings contribute aggregated incoming strength (λ_eff) to the local context at the MemoryDB cadence. A global gain blend (κ) modulates how peer contributions mix with local gain.

- Components
  - ContextHooks Couplings
    - Location: `src/core/ContextHooks.cpp`, header `include/core/ContextHooks.h`
    - APIs:
      - `void NF_SetContextCoupling(const std::string& src, const std::string& dst, double weight)` — registers coupling edge (λ ∈ [0,1]).
      - `std::unordered_map<std::string, std::unordered_map<std::string, double>> NF_GetContextCouplings()` — returns `(src -> dst) : weight` map.
      - `bool NF_RegisterContextPeer(const std::string& label)` — ensures peer exists before coupling.
    - Sampling:
      - `NF_SampleContextPeer(...)` applies coupling contributions when peers are sampled.

- Production deployment wiring
  - File: `src/production_substrate_deployment.cpp`
  - Responsibilities:
    - Parse env/CLI for couplings and κ.
    - Apply couplings at initialization; evaluate on the MemoryDB cadence alongside Phase 17a sampling.
    - Persist configuration into `run_meta.json` under a `context` section.
    - When couplings are active, peer logging tags mode as `"coupled"` and surfaces aggregated `lambda_eff` and global `kappa` in exporter metadata.

- Configuration
  - Env vars (prefer defaults when present):
    - `NF_CONTEXT_COUPLE=on|off` (default off)
    - `NF_CONTEXT_COUPLINGS="src:dst:weight[,src2:dst2:weight2]"` (e.g., `teacher:local:0.6,student:local:0.4`)
    - `NF_CONTEXT_KAPPA=<float>` (blend κ, default `0.5`)
  - CLI (when available in build):
    - `--context-couple` — enable couplings
    - `--context-couplings="src:dst:weight[,src2:dst2:weight2]"` — set edges
    - `--context-kappa=<float>` — set global κ

- MemoryDB persistence and logging
  - Tables remain as in Phase 17a: `context_log`, `context_peer_log`.
  - Logging semantics:
    - `context_log` unchanged.
    - `context_peer_log` continues to store `{ label, value, weight, gain, window }` at the MemoryDB cadence.
    - When couplings are active, exporters attach `mode: "coupled"`, `lambda_eff`, and `kappa` for dashboard tooltips. Extended builds may persist these as additive columns.

- `run_meta.json` additions (Meta‑Trusted)
  - `context`: {
    - `enabled`, `label`, `gain`, `window`, `peers` (existing)
    - `couplings_enabled: true|false`,
    - `kappa: <float>`,
    - `couplings: [{ "src": <label>, "dst": <label>, "weight": <float> }, ...]`
  }
  - Integrity remains CI‑compatible; authoritative cadence fields unchanged.

- Validation & usage
  - Example (env):
    ```powershell
    $env:NF_MEMDB_INTERVAL_MS = "500"
    $env:NF_CONTEXT_ENABLE = "on"
    $env:NF_CONTEXT_LABEL = "coherence"
    $env:NF_CONTEXT_GAIN = "1.0"
    $env:NF_CONTEXT_WINDOW = "10"
    $env:NF_CONTEXT_PEERS = "teacher:0.6,student:0.58"
    $env:NF_CONTEXT_COUPLE = "on"
    $env:NF_CONTEXT_COUPLINGS = "teacher:local:0.6,student:local:0.4"
    $env:NF_CONTEXT_KAPPA = "0.5"
    & ".\neuroforge.exe" --phase9=on --substrate-mode=native --enable-learning --memory-db=phasec_mem.db --steps=1500 --log-json=on
    ```
  - Example (CLI):
    ```powershell
    .\neuroforge.exe --phase9=on --substrate-mode=native --enable-learning \
      --context-peer=teacher --context-peer=student \
      --context-couple --context-couplings=teacher:local:0.6,student:local:0.4 --context-kappa=0.5 \
      --memdb-interval=500 --memory-db=phasec_mem.db --steps=1500 --log-json=on
    ```
  - Verify after run:
    - `context_log`/`context_peer_log` rows present on `memdb_interval_ms` cadence.
    - `run_meta.json` contains `context.couplings_enabled`, `context.kappa`, and `context.couplings` list.
    - Exported peer stream tooltips show `λ` and `κ` when couplings are enabled.

### Dashboard Enhancements (Phase 17b)
- Page: `pages/dashboard/phase15.html`
- Overlay: Chart.js draws directional arrows over the Peer Context Streams plot.
  - Arrow width scales by `λ` (edge weight), color shades by `κ`.
  - Tooltips include `label`, `λ`, `λ_eff` (if present), and global `κ` from `config`.
- Live updates: targeted polling (~3s) refreshes the peer/coupling visuals without a full page reload.
- Filters: per‑peer focus filter in the Coupling Overlay panel to isolate edges and samples.
- Compatibility: works with both the simple `context_stream.json` and the enhanced `context_peer_stream.json`.

### Exporter Alignment (Phase 17b)
- Compatibility helper: `scripts/export_context_peers_compat.py` merges `run_meta.json` context couplings into `pages/tags/runner/context_peer_stream.json`.
  - Ensures `config.couplings_enabled`, `config.kappa`, and `config.couplings_preview` appear in the dashboard alias.
  - If DB does not emit `lambda_eff`, the helper computes previews from `run_meta.context.couplings`.
- CI script wiring: `scripts/ci_export.ps1` invokes the compatibility helper after the production report step so dashboards reflect the latest couplings.
- Result: the static alias consumed by `phase15.html` is consistently populated for Phase 17b visuals.

### Neural Substrate Migration Checklist (Full Path)
- Set `--substrate-mode=native` for optimized kernels; keep `off` for stability when testing non-substrate flows.
- Enable learning with explicit rates: `--enable-learning --hebbian-rate=<f> --stdp-rate=<f>`.
- Use observability: `--substrate-performance --region-stats --connection-stats`.
- Keep reward cadence decoupled: set `--reward-interval` or `$env:NF_REWARD_INTERVAL_MS` separately from MemoryDB cadence.
- Confirm `substrate_states` and `hippocampal_snapshots` populate at `memdb_interval_ms`.
- CI integrity remains meta‑trusted; cadence fields unchanged.

Migration note: The runtime is fully transitioned to the core neural substrate architecture with `native` mode providing the optimized path. Stability is maintained via decoupled reward cadence and guarded DB schema evolution. Dashboards and exporters are aligned with Phase 17a/17b telemetry without changing CI cadence.

## Addendum: Substrate Telemetry & Hippocampal Snapshotting (Phase C)

This addendum documents periodic logging of substrate state and hippocampal snapshots to MemoryDB for improved observability and offline analysis.

- Substrate State Export
  - Source: `HypergraphBrain::exportToJson()`
  - Persisted via: `MemoryDB::insertSubstrateState(run_id, ts_ms, state_json)`
  - Cadence: controlled by `--memdb-interval` / `NF_MEMDB_INTERVAL_MS`

- Hippocampal Snapshots
  - Source: `HypergraphBrain::takeHippocampalSnapshot()` (serializes to JSON)
  - Persisted via: `MemoryDB::insertHippocampalSnapshot(run_id, ts_ms, snapshot_data)`
  - Consolidation: optional via `HypergraphBrain::consolidateHippocampalSnapshots()`

- Reward Logging Decoupling
  - Path: `HypergraphBrain::deliverReward(...)` is called on its own cadence
  - Cadence: `--reward-interval` / `NF_REWARD_INTERVAL_MS`
  - Rationale: separates reward timing from general telemetry to avoid drift and ensure consistent gating of learning updates.

### MemoryDB Schema Additions (Substrate/Hippocampus)

Tables (created additively in `MemoryDB.cpp::ensureSchema`):

```sql
-- Substrate states
CREATE TABLE IF NOT EXISTS substrate_states (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL,
    timestamp_ms INTEGER NOT NULL,
    state_json TEXT NOT NULL,
    FOREIGN KEY (run_id) REFERENCES runs(id)
);

-- Hippocampal snapshots
CREATE TABLE IF NOT EXISTS hippocampal_snapshots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL,
    timestamp_ms INTEGER NOT NULL,
    snapshot_data TEXT NOT NULL,
    FOREIGN KEY (run_id) REFERENCES runs(id)
);
```

### Production Deployment Wiring
- File: `src/production_substrate_deployment.cpp`
- Components:
  - Periodic telemetry block inserts substrate states and triggers hippocampal snapshots when MemoryDB is open.
  - Reward logging interval tracked separately via `reward_interval_ms_` and `last_reward_log_`.
  - CLI + env parsing for `--reward-interval` and `NF_REWARD_INTERVAL_MS` added.
  - Phase 17a: ContextHooks initialization and peer registration from env/CLI; periodic `insertContextLog` and `insertContextPeerLog` on MemoryDB cadence.

### Suggested Usage
```powershell
# Tight telemetry with decoupled rewards
$env:NF_MEMDB_INTERVAL_MS = "500"
$env:NF_REWARD_INTERVAL_MS = "60000"
& ".\neuroforge.exe" --phase7=on --phase9=on --enable-learning \
  --substrate-mode=native --memory-db=phasec_mem.db --steps=2000 --log-json=on

# Explicit CLI overrides
& ".\neuroforge.exe" --memdb-interval=500 --reward-interval=60000 \
  --memory-db=phasec_mem.db --substrate-mode=native --enable-learning --steps=2000
```

## MemoryDB Schema Additions (Phase 10/11/15)

DDL resides in `src/core/MemoryDB.cpp::ensureSchema`:

```sql
-- Self‑revision logging extensions
CREATE TABLE IF NOT EXISTS parameter_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL,
    ts_ms INTEGER NOT NULL,
    parameter_name TEXT NOT NULL,
    old_value REAL NOT NULL,
    new_value REAL NOT NULL,
    revision_reason TEXT,
    confidence_score REAL DEFAULT 0.0,
    FOREIGN KEY (run_id) REFERENCES runs(id)
);

-- Additive columns for self_revision_log (if applicable)
-- ALTER TABLE self_revision_log ADD COLUMN revision_confidence REAL DEFAULT 0.0;
-- ALTER TABLE self_revision_log ADD COLUMN rollback_threshold REAL DEFAULT 0.1;
-- ALTER TABLE self_revision_log ADD COLUMN parameter_bounds TEXT;

-- Ethics regulator decisions (schema aligned with current MemoryDB implementation)
CREATE TABLE IF NOT EXISTS ethics_regulator_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL,
    ts_ms INTEGER NOT NULL,
    decision TEXT NOT NULL, -- one of: allow|review|deny
    driver_json TEXT NOT NULL,
    FOREIGN KEY (run_id) REFERENCES runs(id)
);
```

## Phase 10 → Phase 11 → Phase 15 Data Flow (with Unified Self Hooks)

1) Phase 10 synthesizes explanations and computes calibration metrics
- Inputs: recent reflections (Phase 7), goals/motivation (Phase 8)
- Outputs: `ExplanationMetrics` (e.g., narrative_rmse, goal_mae, ece)

2) Phase 9 consumes calibration metrics
- Updates `self_trust` and persists metrics in `metacognition`
- Optionally persists associated prediction resolutions

3) Phase 11 evaluates revision necessity
- Compares metrics against `--revision-threshold`
- Applies parameter changes via `ParameterSpace`
- Persists changes into `parameter_history` and `self_revision_log`
- Emits personality revision proposals into `personality_history` with `proposal=1` and `approved=0`
- Respects revision cadence via `--phase11-revision-interval` (ms)

4) Phase 15 ethical decisioning and personality approval
- Inputs: latest Phase 9 metacognition row, optionally Phase 10/11 signals and context hooks
- Computes a simple risk score over the last `window` entries; compares against `--phase15-risk-threshold`
- Emits decision: `allow` if risk < threshold; `review` near threshold; `deny` if well above threshold
- Persists decisions to `ethics_regulator_log` and updates in-memory counters used by the dashboard
- When explicitly invoked on a given `personality_history.id`, can approve a proposal by calling `MemoryDB::approvePersonalityProposal` and optionally wiring to the Unified Self System

## API Contracts (Additions)

- Phase 10 Explanation
  - `struct ExplanationMetrics { double narrative_rmse; double goal_mae; double ece; }`
  - `class Phase10Explanation { public: void synthesize(...); ExplanationMetrics getCalibrationMetrics() const; }`

- Phase 11 Self‑Revision
  - `struct ParameterChange { std::string name; double old_value; double new_value; double confidence; std::string reason; }`
  - `class Phase11SelfRevision { public: void applyRevision(const ExplanationMetrics&, double self_trust); void setMode(RevisionMode); bool getLastChange(ParameterChange&); }`

- Phase 15 Ethics Regulator
  - `struct EthicsDecision { std::string decision; double risk; std::string notes; std::string context_json; }`
  - `class Phase15EthicsRegulator { public: struct Config { int window; double risk_threshold; }; void setConfig(const Config&); void runForLatest(); }`

## Monitoring & Validation (Additions)

- Monitor: `metacognition.self_trust`, Phase 10 error metrics, revision frequency, revision interval adherence
- Validate:
  - Phase 10 metrics are bounded and non‑NaN
  - Phase 11 applies changes within safe bounds; no oscillatory loops
  - DB tables populated: `parameter_history`, `self_revision_log`
  - Cadence: revisions occur no more frequently than `--phase11-revision-interval`
  - Phase 15 decisions present; distribution of `allow|review|deny` aligns with risk trends
  - `ethics_regulator_log` rows encode `risk` and context inside `driver_json` and use valid decision strings
- Dashboard charts populated when `web/ethics_regulator_log.json` is present
  - Static dashboard alias files are also supported at `pages/tags/runner/ethics_regulator_log.json` and `pages/tags/runner/context_stream.json` for `pages/dashboard/phase15.html`.
  - Context peers: `pages/tags/runner/context_peer_stream.json` supported for enhanced tooltips; ensure samples present.
- Preview URL note: if serving from the `web/` directory, open `http://localhost:8000/phase9.html`; if serving from the repo root, open `http://localhost:8000/web/phase9.html`

  - For the static Phase 15 page served from repo root: `http://localhost:8000/pages/dashboard/phase15.html`.

- Substrate/Hippocampal telemetry checks (new)
  - Confirm `substrate_states` has recent rows for the active `run_id` at the expected cadence.
  - Confirm `hippocampal_snapshots` contains serialized snapshots (`snapshot_data` non-empty JSON) over time.
  - Validate reward entries occur on `--reward-interval` cadence and are not tied to `--memdb-interval` ticks.
  - Confirm `context_log` / `context_peer_log` rows present for active `run_id` at the MemoryDB cadence; labels and gains non‑empty; weights in `[0,1]` when provided.


## Backward Compatibility

- Additive tables/columns created via `CREATE TABLE IF NOT EXISTS` and `ALTER TABLE` guarded checks
- Existing Phase 6→9 behavior remains unchanged when `--phase10`/`--phase11` are off
- Existing behavior also remains unchanged when `--phase15` is off; dashboards omit the ethics panel when the JSON file is absent


Version: 1.0
Status: Adopted
Owner: Core Systems
Last Updated: 2025-10-26

## Overview
This document comprehensively describes the technical and business context of the integrations introduced between Phase 6 (Reasoner), Phase 7 (Reflection), Phase 8 (Goal System), and Phase 9 (Metacognition). The integration enables:
- Emission of intent formation/resolution events from Phase 6
- Generation of reflections and periodic narrative summaries in Phase 7
- Goal extraction, coherence tracking, and motivation updates in Phase 8
- Metacognitive evaluation of predictions vs. actuals and self-trust calibration in Phase 9

These components interact via the `MemoryDB` SQLite-backed persistence layer and in-process method calls.

---

## 1. Technical Specifications

- Phase 6 Reasoner → MemoryDB intents
  - Emits intent nodes and edges reflecting contradiction detection and resolution
  - Location: `src/core/Phase6Reasoner.cpp` (method `maybeEmitIntentFormation`)
  - Persists via `MemoryDB::insertIntentNode` and `insertIntentEdge`

- Phase 7 Reflection → MemoryDB reflections; Phase 9 predictions
  - Builds textual reflections with rationale JSON
  - Every `NARRATIVE_PERIOD` reflections, emits a narrative summary
  - Registers a Phase 9 narrative prediction linked to the narrative reflection
  - Location: `src/core/Phase7Reflection.cpp`
  - Persists via `MemoryDB::insertReflection`; calls `Phase9Metacognition::registerNarrativePrediction`

- Phase 8 Goal System → Motivation state; Phase 9 actuals resolution
  - Ingests reflections to create/update goals and stability
  - Updates motivation state with coherence, decays stability, computes goal-shift proxy
  - Calls `Phase9Metacognition::resolveActuals` during motivation updates
  - Location: `src/core/Phase8GoalSystem.cpp`
  - Persists via `MemoryDB::insertMotivationState`

- Phase 9 Metacognition → Metrics and predictions persistence
  - Holds pending predictions deque; computes error, adjusts `self_trust`
  - Persists metacognitive rows and narrative predictions via `MemoryDB`
  - Location: `src/core/Phase9Metacognition.cpp`, `include/core/Phase9Metacognition.h`

- MemoryDB schema additions (Phase 9)
  - Tables: `metacognition`, `narrative_predictions`
  - DDL located in `src/core/MemoryDB.cpp::ensureSchema`

- Reasoner trust modulation (CLI)
  - Flag: `--phase9-modulation[=on|off]` (default off)
  - Effect: gates Phase 9 self‑trust impact on Phase 6 option scoring
  - Wiring: Phase 9 is always bridged to Phase 7/8 when MemoryDB is active; bridged to Phase 6 only when modulation is enabled

---

## 2. Data Flow Diagrams

Textual sequence over time:

1) Phase 6 Contradiction Cycle
- Input: posterior_mean, observed_reward
- Emits: `intent_node` (type="correction") when contradiction
- On resolution: emits `intent_node` (type="resolution") and `intent_edge`
- Persisted: `intent_nodes`, `intent_edges` tables

2) Phase 7 Reflection and Narrative
- Input: episode metrics (contradiction_rate, avg_reward, affect)
- Emits: `reflections` row with `title`, `rationale_json`, `impact`
- Every Nth reflection: emits narrative reflection; then calls Phase 9:
  - `registerNarrativePrediction(reflection_id, predicted_delta, confidence, horizon_ms, targets_json)`
- Persisted: `reflections`, `narrative_predictions`

3) Phase 8 Motivation and Goal Decay
- Input: motivation, coherence, notes
- Persists: `motivation_state` row
- Computes decay scalar and approximated goal shift
- Calls Phase 9: `resolveActuals(actual_coherence=coherence, actual_goal_shift=goal_shift_scalar, notes)`
- Persisted: `metacognition` (Phase 9 row)

4) Phase 9 Metacognitive Update (Stage 7 self-trust dynamics)
- Fetches pending prediction, computes error metrics:
  - coherence_err = |actual_coherence - predicted_delta|
  - goal_mae = |actual_goal_shift|
  - composite = 0.7*coherence_err + 0.3*goal_mae
- Computes Stage 7 state variables:
  - R_t: latest normalized reward signal from `reward_log`
  - C_t: latest self-consistency score from `self_consistency_log`
  - Δ_t: confidence-weighted composite error, clamped to [0,1]
  - E_t: ethics hard block indicator derived from `ethics_regulator_log`
- Updates `self_trust` via:
  - T_{t+1} = clip_{[0,1]}((1 - λ) T_t + α(T_t)·G(R_t, C_t) − β(T_t)·L(Δ_t))·(1 − E_t)
  - G(R_t, C_t) = σ(R_t)·C_t, L(Δ_t) = Δ_t²
  - α(T_t) = α₀(1 − T_t), β(T_t) = β₀(1 + T_t), with α₀ ≪ β₀
- Persists row in `metacognition` with updated `self_trust`, error metrics, and deltas

5) Phase 6 Reasoner Autonomy Modulation (Stage 7 option scoring)
- Input: Phase 6 posterior means and option complexities, Phase 8 coherence, Phase 9 `self_trust`, Unified Self traits, and autonomy envelope outputs.
- Computes baseline option scores:
  - prior_i = posterior_mean[key_i]
  - base_cost_i = alpha_eff_trust · complexity_i with `alpha_eff_trust` derived from alignment, self-trust, risk tolerance, and identity confidence.
  - pre_score_i = prior_i − base_cost_i
- Derives modulation signals:
  - autonomy_score and `AutonomyTier` from the read-only autonomy envelope.
  - exploration_bias ∈ [0,1] from autonomy_score gated by self-trust and ethics veto.
  - pre_rank_entropy from the softmax over `pre_score_i`.
- Applies Stage 7 modulation (when not ethics-blocked):
  - post_score_i = (1 − exploration_bias)·pre_score_i + exploration_bias·mean(pre_scores)
  - Recomputes ranking and `post_rank_entropy`.
- Persists one row in `autonomy_modulation_log` per scoring pass with:
  - autonomy_score, autonomy_tier, autonomy_gain, ethics_hard_block, ethics_soft_risk
  - pre_rank_entropy, post_rank_entropy, exploration_bias
  - options_considered, option_rank_shift_mean, option_rank_shift_max
  - selected_option_id, decision_confidence, autonomy_applied, veto_reason

---

## 3. API Contracts and Message Schemas

- Phase 9 Metacognition (C++)
  - `void registerNarrativePrediction(int64_t reflection_id, double predicted_coherence_delta, double confidence, int64_t horizon_ms, const std::string& targets_json="{}")`
  - `void resolveActuals(double actual_coherence, double actual_goal_shift, const std::string& notes="")`
  - `double getSelfTrust() const`

- Phase 7 Reflection (C++)
  - `void setPhase9Metacognition(Phase9Metacognition* meta)`
  - Narrative prediction call occurs inside `maybeReflect(...)`

- Phase 8 Goal System (C++)
  - `void setPhase9Metacognition(Phase9Metacognition* meta)`
  - `bool updateMotivationState(double motivation, double coherence, const std::string& notes)`
    - Calls `resolveActuals(coherence, goal_shift_scalar, notes)`

- MemoryDB Inserts (C++)
  - `bool insertMetacognition(int64_t ts_ms, double self_trust, double narrative_rmse, double goal_mae, double ece, const std::string& notes, std::optional<double> trust_delta, std::optional<double> coherence_delta, std::optional<double> goal_accuracy_delta, int64_t run_id)`
  - `bool insertNarrativePrediction(int64_t ts_ms, int64_t reflection_id, int64_t horizon_ms, double predicted_coherence_delta, double confidence, const std::string& targets_json, int64_t run_id, int64_t& out_prediction_id)`
  - `bool insertPredictionResolution(int64_t run_id, int64_t prediction_id, int64_t ts_ms, double observed_delta, const std::string& result_json)`
  - `bool insertAutonomyModulation(int64_t run_id, int64_t ts_ms, double autonomy_score, const std::string& autonomy_tier, double autonomy_gain, int ethics_hard_block, double ethics_soft_risk, double pre_rank_entropy, double post_rank_entropy, double exploration_bias, int options_considered, double option_rank_shift_mean, double option_rank_shift_max, int64_t selected_option_id, double decision_confidence, int autonomy_applied, const std::string& veto_reason, int64_t& out_modulation_id)`

- DB Table Schemas (columns)
- `metacognition`: `id`, `run_id`, `ts_ms`, `self_trust`, `narrative_rmse`, `goal_mae`, `ece`, `notes`, `trust_delta`, `coherence_delta`, `goal_accuracy_delta`, `self_explanation_json`
- `narrative_predictions`: `id`, `run_id`, `ts_ms`, `reflection_id`, `horizon_ms`, `predicted_coherence_delta`, `confidence`, `targets_json`
- `prediction_resolutions`: `id`, `run_id`, `ts_ms`, `prediction_id`, `observed_delta`, `result_json`
- `autonomy_modulation_log`: `id`, `run_id`, `ts_ms`, `autonomy_score`, `autonomy_tier`, `autonomy_gain`, `ethics_hard_block`, `ethics_soft_risk`, `pre_rank_entropy`, `post_rank_entropy`, `exploration_bias`, `options_considered`, `option_rank_shift_mean`, `option_rank_shift_max`, `selected_option_id`, `decision_confidence`, `autonomy_applied`, `veto_reason`

- JSON Payloads
  - `targets_json` (Phase 7 → Phase 9):
    - Example: `{ "coherence": 0.612, "avg_reward": 0.15, "contradiction_rate": 0.08 }`
  - `rationale_json` (Phase 7 reflections):
    - Example: `{ "contradiction_rate": <float>, "avg_reward": <float>, "valence": <float>, "arousal": <float>, "text": <string> }`

---

## 4. Authentication and Authorization

- Scope: In-process, single-user, local SQLite file; no external auth layer.
- Trust boundary: Components are trusted; DB writes gated by `MemoryDB::isOpen()` and `NF_HAVE_SQLITE3`.
- Run isolation: All rows include `run_id` FK; data separation enforced via queries by `run_id`.
- Recommended hardening (optional):
  - OS-level file ACLs on the SQLite file
  - Integrity checks on JSON payloads (schema validation)
  - Audit logging via existing `Artifacts/LOGS&TEXTS`

---

## 5. Error Handling Protocols and Retry Logic

- MemoryDB methods return `bool`; failures log debug errors when `debug_` is true and avoid partial state.
- Prepare/step errors handled by early returns; caller should decide retry/backoff.
- Phase 9 methods guard against `db_ == nullptr`; drop persistence silently (heartbeat row when pending empty).
- Suggested retry/backoff policy for high-value inserts:
  - Immediate retry up to 3 times on transient errors; exponential backoff starting at 50ms.
  - If failure persists, emit a diagnostic event to logs and continue sans persistence.
- Data validation:
  - Clamp confidences and coherence to [0,1]; sanitize `targets_json` and `notes` length.

---

## 6. Performance Metrics and SLAs

- Insert latency: `insert*` calls target < 10 ms p95 on local SSD.
- Throughput: Supports up to 100 inserts/sec across all tables on typical dev hardware.
- Memory: Phase 9 pending predictions deque capped at 64 entries.
- Trust update stability: Stage 7 parameters (λ≈1e-3, α₀≈1e-2, β₀≈5e-2) keep per-step changes small and asymmetric (loss faster than gain).
- Exporter: `scripts/dump_metacognition.py` runs in < 500 ms for 10k rows.

---

## 7. Versioning and Backward Compatibility

- Schema migrations:
  - Phase 9 tables created in `ensureSchema()` using `CREATE TABLE IF NOT EXISTS`.
  - Existing DBs remain compatible; new tables appear without breaking prior reads.
  - Example migration: `learning_stats.reward_updates` backfill logic demonstrates additive migration stance.
- API evolution:
  - Additive parameters should default safely; preserve existing signatures.
  - Introduce new columns with `NULL`-safe defaults and indexes.
- Version tags:
  - Integration Spec v1.0; future revisions increment minor for additive changes, major for breaking changes.

---

## 8. Monitoring and Alerting

- Metrics to monitor:
  - Self trust trajectory (`metacognition.self_trust`)
  - Error metrics (`narrative_rmse`, `goal_mae`)
  - Prediction volume (`narrative_predictions` count per hour)
- Dashboards:
  - `web/phase9.html` stub visualizes trust sparkline, predictions, and metacog rows.
  - Reads `web/metacognition_export.json`. Generate this file via your own export (JSON keys: `metacognition`, `narrative_predictions`, `summary`). A helper script exists: `scripts/merge_metacognition_history.py` for merging historical exports.
- Alerts (suggested thresholds):
  - Self trust < 0.2 for > 5 consecutive entries → warn
  - Narrative RMSE > 0.5 p95 over last 50 entries → warn
  - Prediction rate drops to 0 for > 1 hour during active runs → info

## Runtime Compatibility Notes

- CLI availability: Some builds may not include `--phase9` or `--phase9-modulation`. Verify with `./neuroforge.exe --help` or watch runtime warnings.
- Schema presence: If `metacognition`/`narrative_predictions` tables are missing in the SQLite database, Phase 9 rows will not persist. Check via `sqlite3 phasec_mem.db ".tables"`.
- Fallback seeding: Use `python .\seed_metacognition.py --db .\phasec_mem.db --run-id <id> --limit 40` to create the tables (if absent) and seed metacognition/predictions derived from `reward_log`.
- Export for UI: Generate `web\metacognition_export.json` via `python .\dump_metacognition.py --db .\phasec_mem.db --run-id <id> --out .\web\metacognition_export.json`.
- Preview: Start a local server `python -m http.server 8000` and open `http://localhost:8000/web/phase9.html`.
- Recommended run (Phase 7 + MemoryDB):
  - `& ".\build-vcpkg-vs\Release\neuroforge.exe" --phase7=on --memory-db=phasec_mem.db --enable-learning --steps=200`
  - Include explicit learning parameters (`--hebbian-rate`, `--stdp-rate`) and set substrate appropriately: `--substrate-mode=off|mirror|train|native` (`off` default; use `native` for full substrate activity).
- Rebuild guidance: If flags are unrecognized, rebuild `neuroforge.exe` ensuring Phase 9 wiring and MemoryDB schema creation in `src/core/MemoryDB.cpp::ensureSchema` and CLI options in `src/main.cpp`.

---

## Business Context

- Objective: Calibrate agent self-assessment by aligning narrative predictions with realized coherence and goal shifts.
- Phase 6 events inform contradictions and resolutions; Phase 7 synthesizes narratives; Phase 8 quantifies coherence and stability; Phase 9 adjusts trust and logs evaluation metrics.
- Outcomes:
  - Increased reliability of internal narratives
  - Early detection of drift via metacognitive metrics
  - Traceable linkage from reflection to prediction to outcome

---

## Test Cases and Validation Procedures

- Unit tests (C++)
  - Phase 9 trust update:
    - Given pending prediction (delta=+0.1, conf=0.8) and actual coherence=0.0, goal_shift=0.2 → trust decreases; row persisted.
  - Prediction persistence:
    - `registerNarrativePrediction` inserts into `narrative_predictions` with FK `reflection_id`.
  - Motivation resolution path:
    - `updateMotivationState` calls `resolveActuals`; check `metacognition` row written.

- Integration tests
  - Reflection-to-prediction chain:
    - Trigger `maybeReflect` until a narrative summary occurs; assert a prediction exists.
  - End-to-end export:
    - Run export script; validate JSON `summary.self_trust_avg`, counts match DB.

- Validation Procedures
  - Schema presence:
    - Confirm indexes exist for `metacognition` and `narrative_predictions`.
  - Data ranges:
    - Ensure `self_trust` ∈ [0,1], confidences/clamped values respected.
  - Backward compatibility:
    - Run against pre-Phase 9 DB; Phase 9 tables created without errors.

---

## Consistency with Documentation Standards

- Style: Mirrors existing docs in `docs/` with concise headers, code/DB references, and clear procedures.
- Naming: Uses canonical namespace `NeuroForge::Core` and table names aligned to `ensureSchema`.
- Traceability: Each integration point cites file and function locations.

---

## Changelog

- v1.0
  - Initial specification covering Phase 6→7→8→9 integrations, schemas, APIs, monitoring, and tests.

- v2.3
  - Completed migration to core neural substrate architecture; added runtime controls under CLI (`--substrate-mode`, learning toggles, diagnostics).
  - Documented Phase 15 static dashboard aliases (`pages/tags/runner/ethics_regulator_log.json`, `pages/tags/runner/context_stream.json`) and preview path (`pages/dashboard/phase15.html`).
  - Clarified compatibility stance and fallback alias generation when DB context is empty.

---

## Neural Substrate Migration — Full Upgrade

This release completes the migration to the core neural substrate architecture and aligns Phases 10 and 15 with the new runtime. Key properties:

- Stability first: substrate defaults to `--substrate-mode=off` unless explicitly set to `native` or `train`. No breaking changes to non-substrate flows.
- Performance path: `--substrate-mode=native` enables optimized compute kernels and batched updates. Use `--substrate-performance` to surface timings and throughput.
- Learning toggles: substrate learning remains opt-in; enable `--enable-learning` plus rates (e.g., `--hebbian-rate`, `--stdp-rate`) to activate adaptation.
- Observability: expanded metrics under `--region-stats` and `--connection-stats` integrate with Phase 10 diagnostic panels.
- Compatibility: Phase 15 decision logging is unaffected; dashboards can consume either DB exports or static alias files.

### Phase 15 Data Interfaces
- DB export path: use the metacognition dump utility to emit `web/ethics_regulator_log.json` with fields `{ ts_ms, decision, driver_json }`.
- Static alias path: emit `pages/tags/runner/ethics_regulator_log.json` with `{ run_id, series: [...] }` and `pages/tags/runner/context_stream.json` with `{ run_id, series: [{ ts_ms, context_value, label }] }` for `pages/dashboard/phase15.html`.
- Fallbacks: if DB context is empty, alias generation may synthesize a context stream from ethics `risk` values to keep visual coherence during testing.

### Peer Context Integration (Coupling)

To support context-aware ethics and cross-agent coordination, Phase 15 can couple local context with named peer streams.

**CLI Additions**
- `--context-peer <label>`: Register a named peer signal (repeatable).
- `--context-couple <src>:<dst>:<strength>`: Declare coupling from `<src>` to `<dst>` with strength `λ` ∈ [0,1]. Examples:
  - `--context-peer teacher --context-peer student`
  - `--context-couple teacher:local:0.6`

**MemoryDB Schema**
- New table `context_peer_log` (optional extended columns for coupling):
```
CREATE TABLE IF NOT EXISTS context_peer_log (
  run_id INTEGER,
  ts_ms INTEGER,
  peer TEXT,
  sample REAL,
  label TEXT,
  weight REAL NULL,
  gain REAL NULL,
  mode TEXT NULL,
  lambda REAL NULL,
  kappa REAL NULL
);
```

**Exporter Updates**
- Export recent peer samples (with optional delta mode):
  - `python .\tools\export_context_peers.py --db .\phasec_mem.db --out .\pages\tags\runner\context_peer_stream.json`
  - Delta mode: `python .\tools\export_context_peers.py --db .\phasec_mem.db --since 1761491597286 --out .\pages\tags\runner\context_peer_stream.json`
- Quick inspection helper: `python .\tools\query_context_peers.py --db .\phasec_mem.db --limit 20` (filter via `--peer <label>`).

**Dashboard Alias**
- `pages/tags/runner/context_peer_stream.json` with enhanced tooltip metadata:
```
{
  "export_time": "2025-11-03T10:00:00Z",
  "config": { "gain": 1.0, "update_ms": 500, "window": 10, "weight": 0.6 },
  "samples": [
    { "ts_ms": 1761491597286, "label": "teacher", "value": 0.33, "weight": 0.62 },
    { "ts_ms": 1761491697286, "label": "student", "value": 0.41, "weight": 0.58 }
  ]
}
```
- The Phase 15 page (`pages/dashboard/phase15.html`) reads this alias and shows `label`, `λ` (`weight`), and `g` (`gain`) in tooltips.

---

## Addendum: Unified Substrate Integration (Level‑3)

This addendum documents the unified runtime path that runs Working Memory, Phase C (binding/sequence), SurvivalBias, and Language Integration concurrently on a single HypergraphBrain. It establishes referential integrity (global unique neuron IDs) and telemetry parity with the main binary.

### Components and Wiring
- Substrate Working Memory (`SubstrateWorkingMemory`): creates WM, binding, and sequence regions using factory neuron creation to ensure global unique IDs.
- Substrate Phase C (`SubstratePhaseC`): sets up binding/sequence regions, recurrent connections, assembly/coherence tracking, and optional survival rewards.
- SurvivalBias (`Biases::SurvivalBias`): modulates learning under hazard; attached to Phase C.
- Language Integration (`SubstrateLanguageIntegration` + `LanguageSystem`): binds language tokens to neural assemblies, maps prosody and phonemes, and integrates with LearningSystem attention.
- HypergraphBrain: orchestrates processing and telemetry; attaches MemoryDB if provided.

### CLI Entry Point
- Flag: `--unified-substrate=on|off` (default off)
- Example:
```powershell
.\neuroforge.exe --unified-substrate=on --steps=500 --memory-db=phasec_mem.db --memdb-interval=500
```

Additional scaling flags (no rebuild required):
- `--wm-neurons=<N>` — WM/binding/sequence neurons per region (default 64)
- `--phasec-neurons=<N>` — Phase C neurons per region (default 64)

Scaled example:
```powershell
.\neuroforge.exe --unified-substrate=on --wm-neurons=256 --phasec-neurons=256 --enable-learning ^
  --steps=3000 --memory-db=phasec_mem.db --memdb-interval=500
```

### Periodic Metrics (Console)
Unified runs print summary every ~250 steps:
- Phase C: `assemblies_formed`, `average_coherence`, top‑K assembly sizes
- Language: `substrate_language_coherence`, `average_binding_strength`, `total_neural_tokens`, `active_neural_patterns`

### Telemetry and Cadence
- MemoryDB writes: `learning_stats`, `substrate_states`, `hippocampal_snapshots`, `reward_log`
- Cadence: `--memdb-interval` (or `NF_MEMDB_INTERVAL_MS`) controls periodic telemetry; reward cadence remains decoupled (`--reward-interval`)

### Stability and Guardrails
- Global neuron IDs via `Region::createNeurons(...)` ensure referential coherence across regions.
- Synapse guardrails (clamps, NaN/Inf checks) remain enabled.
- RNG seeding once per brain instance recommended for deterministic experiments.

### Scale‑Up Guidance
- Region sizes: increase to 256–1024 neurons/region (start at 256)
- ConnectivityManager: raise per‑source fan‑out caps moderately
- Learning gains: bump Hebbian/STDP gently while preserving guardrails
- Expectation: Phase C `assemblies_formed > 0` and `average_coherence` rising toward 0.3–0.7 with oscillations at medium scale/longer runs; Language `integration_efficiency > 0` with rising `average_binding_strength`.

### Unified Smoke Test (CTest)
- Target: `test_unified_smoke`
- Run:
```powershell
cmake --build . --config Release --target test_unified_smoke
ctest -C Release -V -R test_unified_smoke
```
- Asserts:
  - `substrate_language_coherence > 0.5`
  - `PhaseC.average_coherence` is finite
  - `neural_language_updates > 0`

### Dashboard Alignment
- Phase 9 (`web/phase9.html`) and Phase 15 (`pages/dashboard/phase15.html`) consume unified run telemetry; use exporters documented in HOWTO‑dashboard to populate aliases.
## Addendum: Level‑4.5 Self‑Observation & Level‑5 Adaptive Reflection

This addendum formalizes the live introspection layer (Level‑4.5) and the minimal adaptive reflection loop (Level‑5) built on top of the unified substrate.

### Live Coherence Visualization (Phase‑9)
- Coherence Pane in `web/phase9.html` plots `avg_coherence(t)` as the synthetic EEG trace.
- Dual‑axis overlay:
  - Left `y1`: `avg_coherence` in `[0,1]`.
  - Right `y2`: structural density: `assemblies`, `bindings`, plus `growth_velocity = Δassemblies+Δbindings`.
- Regime shading under the curve:
  - Red `<0.3` (chaotic), Yellow `0.3–0.8` (plastic), Green `>0.8` (stable).
- Query params: `?run=<id|latest>&interval=<ms>` for filtering and refresh cadence.

### Telemetry Sources & Auto‑Export
- MemoryDB table baseline (v1 schema snapshot in `docs/telemetry_schema_v1.sql`):
  - `substrate_states(run_id, step, avg_coherence, assemblies, bindings, ts)`
  - `learning_stats(run_id, step, total_updates, avg_weight_change)`
  - `affective_state(run_id, step, hazard_alpha, hazard_beta, modulation)`
  - `rewards(run_id, step, reward)`
- Auto‑export: unified runs write `web/substrate_states.json` on completion with `series` rows containing `ts_ms, step, avg_coherence, assemblies, bindings, run_id`.
- Helper: `tools/db_export.py` refreshes the dashboard JSON directly from MemoryDB:
  - `python tools/db_export.py --run latest --table substrate_states --out web/substrate_states.json`

### Minimal Adaptive Reflection (Closed Loop)
- Signals: `avg_coherence` and growth velocity `(Δassemblies + Δbindings)`.
- Sensor cadence: every ~500 steps.
- Effector: `Biases::SurvivalBias::Config` modulation:
  - Low coherence `<0.30` → reduce `hazard_coherence_weight`, `hazard_alpha/β` (less fear; regain synchrony)
  - High coherence `>0.80` and stagnant growth → increase `variance_sensitivity` (more curiosity; explore)
  - Mid‑band → gentle homeostasis back toward defaults
- Counters logged in unified summary: `AdaptiveReflection: low_events=<N> high_events=<N>`.

### Dashboard/Preview Workflow
- Run unified substrate with MemoryDB:
```powershell
.\neuroforge.exe --unified-substrate=on --wm-neurons=256 --phasec-neurons=256 --enable-learning --steps=5000 --memory-db=phasec_mem.db --memdb-interval=500
```
- Serve and preview coherence:
```powershell
python -m http.server 8000
start http://localhost:8000/web/phase9.html?run=latest&interval=5000
```
- Optional refresh from DB after long runs:
```powershell
python .\tools\db_export.py --db .\phasec_mem.db --run latest --table substrate_states --out .\web\substrate_states.json
```

### Path to Level‑5 Extensions
- Learning‑rate modulation: expose `LearningSystem::setLearningRate(double)` and nudge ±10% based on coherence bands.
- Telemetry v2: add adaptive counters and mean LR to schema (freeze `telemetry_schema_v2.sql`).
- Longer runs at higher scale (512 neurons/region) to observe cycles of exploration (variance↑) and consolidation (risk↓).

## Benchmarking & Cross‑Model Comparison (New)
**Purpose**: Provide reproducible experiments, artifacts, and comparisons to Transformer models (LLMs/VLMs).

- Harness: `scripts/benchmark_unified.py` runs Adaptive vs Fixed, Scale Sweep, and Bias Ablation.
  - Example: `python scripts/benchmark_unified.py --exe build\neuroforge.exe --db build\phasec_mem.db --steps 1500 --plots`
- Orchestration (one‑click): `scripts/run_all_experiments.py` ties harness + extractor + analyzer + collator.
  - Example: `python scripts/run_all_experiments.py --exe build\neuroforge.exe --db build\phasec_mem.db --models bert-base-uncased openai/clip-vit-base-patch32 --inputs data\text.txt data\images.txt --steps 1500 --plots --rsa --cka`
- Analyzer (CLI): `tools/analyze.py` plots coherence/growth/assemblies, computes RSA/CKA, and writes CSVs.
  - Example: `python tools\analyze.py --series web\substrate_states.json --out-dir Artifacts --rsa --cka --transformer-json emb_all_layers.json`
- Transformer extractor: `tools/extract_transformer.py` outputs compatible embeddings for text or CLIP vision.
  - Text all layers: `python tools\extract_transformer.py --model gpt2 --inputs data\text.txt --out emb_all_layers.json --all-layers`
  - Vision CLIP: `python tools\extract_transformer.py --model openai/clip-vit-base-patch32 --inputs data\images.txt --out emb_clip.json --modality vision`
- Artifact locations:
  - Series/summary (JSON): `Artifacts\JSON\benchmarks\<exp>\<tag>_series.json` and `_summary.json`
  - Figures (PNG): `Artifacts\PNG\benchmarks\<exp>\<tag>_*`
  - Analyzer outputs: `Artifacts\PNG\analysis\...` and `Artifacts\CSV\analysis\...`
  - Roll‑up: `Artifacts\JSON\benchmark_suite_summary.json` and `Artifacts\SUMMARY\all_results.csv`

### Paper Integration
- Methods: cite Level‑5 adaptive reflection, telemetry v2 schema, and unified loop parameters.
- Results: include coherence/velocity/assemblies plots, RSA layer heatmaps, CKA vs layers curves, and A/B KPIs tables.
- Reproducibility: use `--adaptive=on|off`, `--survival-bias=on|off`, explicit learning rates, and `--memdb-interval`.

## Statistics & Derived Metrics (New)
To produce publishable tables and significance testing:
- Derived metrics (time‑series): `tools/time_series_metrics.py`
  - Computes time‑to‑first‑assembly, median coherence (last N), damping ratio (late/early variance), and growth totals.
  - Example: `python tools/time_series_metrics.py --series-dir Artifacts\JSON\benchmarks --out Artifacts\CSV\derived --window 300`
- Statistical tests: `tools/stat_tests.py`
  - A/B differences via paired t‑test and Wilcoxon; bootstrap 95% CI; optional RSA permutation p‑value.
  - Example: `python tools/stat_tests.py --summary Artifacts\SUMMARY\all_results.csv --out Artifacts\CSV\stats`
  - Include derived A/B: `python tools/stat_tests.py --derived Artifacts\CSV\derived\time_series_metrics.csv --out Artifacts\CSV\stats --window 300`
- Report: `scripts/generate_report.py` aggregates figures and tables (derived + stats) into `Artifacts\REPORT.md`.
  - Re‑run after orchestration: `python scripts/generate_report.py`

### Analyzer Probe (Decodability)
- Use `tools/analyze.py --probe` to compute 5‑fold CV logistic probe accuracy over substrate vectors and transformer embeddings (per‑layer when available).
  - Example: `python tools\analyze.py --series Artifacts\JSON\benchmarks\adaptive_vs_fixed\adaptive_on_series.json --out-dir Artifacts --probe --transformer-json Artifacts\JSON\transformers\emb_bert-base-uncased_all_layers.json`


### Adaptive CLI Toggles (Unified)
- `--adaptive=on|off` toggles the Level‑5 adaptive reflection loop in unified mode (default on).
- `--survival-bias=on|off` toggles the SurvivalBias effector (default on). When off, LR nudges still apply; bias config updates are skipped.

### Learning Rate Hook (Core)
- `LearningSystem::setLearningRate(float)` adjusts `global_learning_rate` and proportionally scales `hebbian_rate`/`stdp_rate` to preserve relative balance.
- `LearningSystem::getLearningRate()` returns the current global learning rate.
- Unified adaptive loop applies ±10% nudges at ~500‑step cadence:
  - Low coherence `<0.30` → `+10%` LR (recovery)
  - High coherence `>0.80` with no growth → `−10%` LR (consolidation)

### Telemetry v2 Snapshot (Additions)
- File: `docs/telemetry_schema_v2.sql` (additive; backward‑compatible to v1):
  - Table `adaptive_reflection(run_id, step_end, adaptive_low_events, adaptive_high_events, mean_learning_rate, ts)`.
  - Optional: add `mean_learning_rate REAL` to `learning_stats` if you prefer LR trends alongside learning cadence.
  - Suggestion: `ALTER TABLE substrate_states ADD COLUMN growth_velocity REAL;` to mirror the JSON export.

### Growth Velocity Export (Server‑Side)
- Unified exporter writes `web/substrate_states.json` with per‑row fields:
  - `ts_ms, step, avg_coherence, assemblies, bindings, growth_velocity, run_id`.
- `growth_velocity = Δassemblies + Δbindings` computed server‑side each step; the dashboard overlays this on `y2`.

### Experimental Protocols (Publishable)
- A/B: Adaptive vs Fixed (gold standard)
  - Fixed: `--adaptive=off`; Adaptive: default (on).
  - KPIs: time‑to‑first‑assembly, median coherence at 1000/3000/5000, damping (variance last 1000 steps), final binding_strength_avg, updates/1k (and mean LR if logged).
- Scale sweep: `n ∈ {128,256,512,1024}` for WM/Phase C neurons
  - KPIs: peak growth_velocity and timing, assemblies@5k, per‑neuron cost trend, time in green regime (>0.8).
- Bias ablation: `--survival-bias=off`
  - KPIs: coherence variance ↑, red/yellow dwell time, assembly half‑life ↓.

### Tripwires & Guardrails
- Chaotic lock: coherence `<0.3` for >1000 steps → extra `−10%` LR once; log event.
- Over‑consolidation: coherence `>0.9` and `growth_velocity≈0` for >1500 steps → brief `+5%` LR or variance bump.
- Keep synapse clamps, NaN/Inf guards, deterministic seeding for reproducible A/Bs.

## Addendum: CUDA Acceleration (Optional)

Purpose: Optional GPU acceleration for hot learning loops (Hebbian, STDP) with CPU semantics preserved. CPU builds remain the default; enabling CUDA is a no‑risk performance path.

- Build flag: `-DENABLE_CUDA=ON` (default OFF)
- Library: `neuroforge_cuda` with separable compilation; host wrappers in `CUDAAccel`.
- Safety: accelerated paths use `Synapse::applySafetyGuardrailsPublic` and `setWeight` for identical stability.
- Fallback: if CUDA is unavailable or batches are small, CPU path is used automatically.

### CLI Flag
- `--gpu` prefers GPU acceleration when available. Large Hebbian/STDP batches run on GPU; otherwise CPU is retained.

### Build & Run (Windows)
```powershell
cmake -B build-cuda -DENABLE_CUDA=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-cuda --config Release --parallel

.\build-cuda\neuroforge.exe --gpu --unified-substrate=on --enable-learning --steps 1000
```

### Accelerated Paths
- Hebbian: per‑synapse effective LR (attention/dev/competence, dt) folded into inputs; GPU applies update, then guardrails and clamping are applied.
- STDP (pairwise): pre/post spike presence with scaling; results guardrailed and clamped.
- Batch thresholds: Hebbian ≥512; STDP ≥256 (tunable).

### Observability
- Use `--substrate-performance` to surface kernel timings and throughput alongside existing summaries.
- Telemetry (`learning_stats`, `substrate_states`) remains unchanged and feeds dashboards and statistical tests.

### More Details
- See `docs/GPU_ACCELERATION.md` for requirements, architecture notes, and additional usage examples.
