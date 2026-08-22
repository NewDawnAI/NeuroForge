# MemoryDB Schema Reference

Source: `src/core/MemoryDB.cpp::ensureSchema`
Timestamp: 2025-12-18

## Tables
- runs: id, started_ms, metadata_json
- run_events: id, run_id, ts_ms, step, type, message, exit_code, rss_mb, gpu_mem_mb
- experiences: id, run_id, ts_ms, step, tag, input_json, output_json, significant
- episodes: id, run_id, name, start_ms, end_ms
- episode_stats: episode_id, steps, success, episode_return
- learning_stats: id, run_id, ts_ms, step, processing_hz, total_updates, hebbian_updates, stdp_updates, reward_updates, avg_weight_change, consolidation_rate, active_synapses, potentiated_synapses, depressed_synapses, avg_energy, metabolic_hazard
- reward_log: id, run_id, ts_ms, step, reward, source, context_json
- substrate_states: id, run_id, ts_ms, step, state_type, region_id, serialized_data
- hippocampal_snapshots: id, run_id, ts_ms, step, priority, significance, snapshot_data
- context_log: id, run_id, ts_ms, sample, gain, update_ms, window, label
- context_peer_log: id, run_id, ts_ms, peer, sample, gain, update_ms, window, label, mode, lambda, kappa
- goal_nodes: goal_id, run_id, description, priority, stability, origin_reflection_id, created_ts_ms, expires_ts_ms, reaffirm_n, vetoed, veto_reason
- goal_edges: goal_id, subgoal_id, weight
- motivation_state: id, run_id, ts_ms, motivation, coherence, notes
- metacognition: id, run_id, ts_ms, self_trust, narrative_rmse, goal_mae, ece, notes, trust_delta, coherence_delta, goal_accuracy_delta, self_explanation_json
- narrative_predictions: id, run_id, ts_ms, reflection_id, horizon_ms, predicted_coherence_delta, confidence, targets_json
- prediction_resolutions: id, run_id, ts_ms, prediction_id, observed_delta, result_json
- self_consistency_log: id, run_id, ts_ms, consistency_score, notes, window_json, driver_explanation
- autonomy_credit_log: id, run_id, ts_ms, credit_value, decay_rate, driver_json
- autonomy_envelope_log: id, run_id, ts_ms, decision, driver_json
- autonomy_modulation_log: id, run_id, ts_ms, autonomy_score, autonomy_tier, autonomy_gain, ethics_hard_block, ethics_soft_risk, pre_rank_entropy, post_rank_entropy, exploration_bias, options_considered, option_rank_shift_mean, option_rank_shift_max, selected_option_id, decision_confidence, autonomy_applied, veto_reason
- self_revision_log: id, run_id, ts_ms, revision_json, driver_explanation, trust_before, trust_after
- self_revision_outcomes: revision_id, run_id, eval_ts_ms, outcome_class, trust_pre, trust_post, prediction_error_pre, prediction_error_post, coherence_pre, coherence_post, reward_slope_pre, reward_slope_post
- parameter_history: id, run_id, ts_ms, phase, parameter, value, revision_id
- preference_memory: id, run_id, key, preferred_value, strength01, evidence_n, beneficial_n, harmful_n, updated_ts_ms
- language_grounding_map: id, run_id, ts_ms, step, stage, token, internal_state_json, correlation, description_length_delta, source
- language_audit_log: id, run_id, ts_ms, step, stage, event, token, details_json, wrote_preference_memory, wrote_goal_nodes, wrote_autonomy_credit, wrote_identity_vector, allowed
- self_concept: id, run_id, ts_ms, step, identity_vector_json, confidence, notes
- personality_history: id, run_id, ts_ms, step, trait_json, proposal, approved, source_phase, revision_id, notes

## Indices
- experiences(run_id, ts_ms)
- learning_stats(run_id, ts_ms)
- substrate_states(run_id, ts_ms)
- metacognition(run_id, ts_ms)
- motivation_state(run_id, ts_ms)
- self_revision_log(run_id, ts_ms)
- self_revision_outcomes(run_id, eval_ts_ms)
- autonomy_credit_log(run_id, ts_ms)
- goal_nodes(run_id)
- goal_nodes(description)
- preference_memory(run_id)
- preference_memory(strength01)
- language_grounding_map(run_id, ts_ms)
- language_grounding_map(token)
- language_audit_log(run_id, ts_ms)
- language_audit_log(event)

## Migrations
- v1→v2: split snapshots into substrate_states and snapshots; added autonomy_envelope_log

## Retention & Performance
- Retain runs ≥ 30 days; aggregate learning_stats into hourly buckets
- Ensure WAL mode; create indices above; batch inserts for telemetry streams

## Contracts
- All timestamps are integer milliseconds since Unix epoch (UTC)
- payload/state_blob use compact binary or JSON per configuration

## Explainability Payloads

### `self_revision_log.driver_explanation` (Phase 11)

`driver_explanation` is currently persisted as a human-readable string describing what triggered the revision and what deltas were applied. It is intended to be auditable and grep-friendly, not a strict JSON schema.

### `metacognition.self_explanation_json` (Phase 10)

`self_explanation_json` is a structured narrative generated for a metacognition row. In Stage 7.5 it may include a `stage7_5` object with the latest evaluated self-revision outcome (when available), e.g.:
```json
{
  "...": "...",
  "stage7_5": {
    "revision_id": 123,
    "eval_ts_ms": 1734480000000,
    "outcome_class": "Neutral",
    "trust_pre": 0.52,
    "trust_post": 0.51,
    "prediction_error_pre": 0.21,
    "prediction_error_post": 0.22,
    "coherence_pre": 0.63,
    "coherence_post": 0.61,
    "reward_slope_pre": 0.0,
    "reward_slope_post": 0.0
  }
}
```

In Stage C v1 it may also include a `stage_c_v1` object describing the governance-only autonomy cap derived from recent self-revision outcomes (when available), e.g.:
```json
{
  "...": "...",
  "stage_c_v1": {
    "revision_reputation": 0.50,
    "autonomy_cap_multiplier": 0.75,
    "window_n": 20,
    "note": "Autonomy constrained due to recent neutral/harmful self-revision outcomes"
  }
}
```

In Stage C v2 it may also include a `stage_c_v2` object describing the governance-only autonomy cap derived from harm-risk and `Autonomy Credit` (when available), e.g.:
```json
{
  "...": "...",
  "stage_c_v2": {
    "p_harm_mean": 0.12,
    "p_harm_ub95": 0.28,
    "harm_risk_cap_multiplier": 1.0,
    "autonomy_credit": 0.72,
    "autonomy_credit_cap_multiplier": 1.0,
    "autonomy_cap_multiplier": 1.0,
    "window_n": 200,
    "beneficial_n": 18,
    "harmful_n": 2,
    "neutral_n": 180,
    "note": "Autonomy unconstrained by harm-risk and autonomy credit"
  }
}
```

In Stage C v3 it may also include a `stage_c_v3` object describing the same governance-only autonomy cap as v2 plus preference stabilization telemetry (when available), e.g.:
```json
{
  "...": "...",
  "stage_c_v3": {
    "p_harm_mean": 0.12,
    "p_harm_ub95": 0.28,
    "harm_risk_cap_multiplier": 1.0,
    "autonomy_credit": 0.72,
    "autonomy_credit_cap_multiplier": 1.0,
    "autonomy_cap_multiplier": 1.0,
    "window_n": 200,
    "beneficial_n": 18,
    "harmful_n": 2,
    "neutral_n": 180,
    "preference_rigidity01": 0.65,
    "preference_destabilization01": 0.10,
    "preference_active_n": 5,
    "note": "Autonomy unconstrained; 5 active preferences"
  }
}
```

In Stage C v4 it may also include a `stage_c_v4` object describing the same governance-only autonomy cap as v3 plus bounded goal formation telemetry (when available), e.g.:
```json
{
  "...": "...",
  "stage_c_v4": {
    "p_harm_mean": 0.12,
    "p_harm_ub95": 0.28,
    "harm_risk_cap_multiplier": 1.0,
    "autonomy_credit": 0.72,
    "autonomy_credit_cap_multiplier": 1.0,
    "autonomy_cap_multiplier": 1.0,
    "window_n": 200,
    "beneficial_n": 18,
    "harmful_n": 2,
    "neutral_n": 180,
    "preference_rigidity01": 0.65,
    "preference_destabilization01": 0.10,
    "preference_active_n": 5,
    "goal_candidate_n": 3,
    "goal_created_n": 2,
    "goal_reaffirmed_n": 1,
    "goal_governance_veto": false,
    "goal_ttl_ms": 604800000,
    "note": "Autonomy unconstrained by harm-risk and autonomy credit; goals eligible for formation"
  }
}
```

In Stage C v5, scope-gated learning decisions are auditable via `language_audit_log`. The runtime records scope open/allow/block events (e.g., `scope_open`, `scope_allow`, `scope_block`) with `allowed=1|0` and a `details_json` payload describing the subsystem and scope key.

## Updates (v0.16+)
- Added `reward_log` table: `id, run_id, ts_ms, step, reward, source, context_json`
  - Records unified and Phase A reward events with structured context metadata.
- Extended `substrate_states` with typed entries: `ts_ms, step, state_type, region_id, serialized_data, run_id`
  - `state_type` includes `phase_a_teacher` and `phase_a_student` for Phase A snapshots.
  - `serialized_data` is JSON, typically `{ "vec": [..], "teacher_id"|"content_id": "..." }`.
- Added `autonomy_modulation_log` table to capture Stage 7 autonomy perturbations to Phase 6 option rankings (`run_id, ts_ms, autonomy_score, autonomy_tier, autonomy_gain, ethics_hard_block, ethics_soft_risk, pre_rank_entropy, post_rank_entropy, exploration_bias, options_considered, option_rank_shift_mean, option_rank_shift_max, selected_option_id, decision_confidence, autonomy_applied, veto_reason`).

### Event Types (Phase A)
- `experiences.event='triplet_ingestion'`
  - Emitted per dataset item; payload includes `image`, `audio`, `caption`, `tokens[]`, and `teacher_id`.
  - References: `src/main.cpp` (search for `triplet_ingestion` and `dataset-mode=triplets`).
- `experiences.event='snapshot:phase_a'`
  - Emitted on autonomous loop with `phase_a.last_*` metrics and `teacher_id`.
  - References: `src/main.cpp` (search for `snapshot:phase_a`).

### Typed Substrate States (Phase A)
- `state_type='phase_a_norms'`
  - Logs teacher/student/projected norms and dimensions for debugging.
  - Reference: `src/core/PhaseAMimicry.cpp:486`–`497`.
- `state_type='phase_a_teacher'`, `state_type='phase_a_student'`
  - Embedding snapshots; content keyed by `teacher_id`/`content_id`.

## Indices (Extended)
- reward_log(run_id, ts_ms)
- substrate_states(run_id, ts_ms, state_type)
