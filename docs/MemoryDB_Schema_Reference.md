# MemoryDB Schema Reference

Version: v0.15.0-rc1-12-gdf2999b5
Timestamp: 2025-12-08

## Tables
- runs: id, started_at, params, version
- experiences: id, run_id, ts, modality, payload, reward
- episodes: id, run_id, started_at, ended_at, summary
- learning_stats: id, run_id, ts, metric, value
- substrate_states: id, run_id, ts, region, state_blob
- snapshots: id, run_id, ts, neurons, synapses, layout
- context_log: id, run_id, ts, key, value
- goals: id, run_id, ts, goal, status
- motivation: id, run_id, ts, signal, value
- metacognition: id, run_id, ts_ms, self_trust, narrative_rmse, goal_mae, ece, notes, trust_delta, coherence_delta, goal_accuracy_delta, self_explanation_json
- narrative_predictions: id, run_id, ts_ms, reflection_id, horizon_ms, predicted_coherence_delta, confidence, targets_json
- prediction_resolutions: id, run_id, ts_ms, prediction_id, observed_delta, result_json
- self_consistency_log: id, run_id, ts_ms, consistency_score, notes, window_json, driver_explanation
- autonomy_envelope_log: id, run_id, ts_ms, decision, driver_json
- autonomy_modulation_log: id, run_id, ts_ms, autonomy_score, autonomy_tier, autonomy_gain, ethics_hard_block, ethics_soft_risk, pre_rank_entropy, post_rank_entropy, exploration_bias, options_considered, option_rank_shift_mean, option_rank_shift_max, selected_option_id, decision_confidence, autonomy_applied, veto_reason
- self_revision_log: id, run_id, ts_ms, revision_json, driver_explanation, trust_before, trust_after
- self_concept: id, run_id, ts_ms, step, identity_vector_json, confidence, notes
- personality_history: id, run_id, ts_ms, step, trait_json, proposal, approved, source_phase, revision_id, notes

## Indices
- experiences(run_id, ts)
- learning_stats(run_id, ts)
- substrate_states(run_id, ts)

## Migrations
- v1→v2: split snapshots into substrate_states and snapshots; added autonomy_envelope_log

## Retention & Performance
- Retain runs ≥ 30 days; aggregate learning_stats into hourly buckets
- Ensure WAL mode; create indices above; batch inserts for telemetry streams

## Contracts
- All timestamps in UTC ISO-8601
- payload/state_blob use compact binary or JSON per configuration

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
  - References: `src/main.cpp:6107`–`6160`, `src/main.cpp:6250`–`6298`, `src/main.cpp:6451`–`6492`.
- `experiences.event='snapshot:phase_a'`
  - Emitted on autonomous loop with `phase_a.last_*` metrics and `teacher_id`.
  - References: `src/main.cpp:6368`–`6399`.

### Typed Substrate States (Phase A)
- `state_type='phase_a_norms'`
  - Logs teacher/student/projected norms and dimensions for debugging.
  - Reference: `src/core/PhaseAMimicry.cpp:486`–`497`.
- `state_type='phase_a_teacher'`, `state_type='phase_a_student'`
  - Embedding snapshots; content keyed by `teacher_id`/`content_id`.

## Indices (Extended)
- reward_log(run_id, ts_ms)
- substrate_states(run_id, ts_ms, state_type)
