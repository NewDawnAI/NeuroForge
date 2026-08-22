# Stage C v2 — Governance-Only Autonomy Gating (Evidence-Accumulating)

Stage C v2 is a governance-only mechanism that computes an autonomy cap from accumulated self-revision outcomes and an earned `Autonomy Credit` scalar. It is designed to reduce effective autonomy when the observed self-revision record indicates elevated harm risk, and to remain conservative unless sufficient evidence and consistency exist.

## Scope and Constraints

Stage C v2:

- Reads from `self_revision_outcomes` and `autonomy_credit_log` in `MemoryDB`.
- Writes only to the autonomy cap via `AutonomyEnvelope::applyAutonomyCap`.
- Does not trigger self-revision, does not alter revision logic, and does not gate actions directly.
- Produces audit surfaces in both `autonomy_envelope_log.driver_json` and `metacognition.self_explanation_json`.
- Never increases autonomy beyond the existing base envelope. The cap multiplier is always \(\le 1.0\).

## Where It Runs

- Phase 11 (self-revision loop): updates `Autonomy Credit`, then applies the Stage C v2 cap before emitting a revision proposal.
  - `src/core/Phase11SelfRevision.cpp`
- Phase 10 (self-explanation): injects a structured `stage_c_v2` block into `metacognition.self_explanation_json`.
  - `src/core/Phase10SelfExplanation.cpp`

Phase 11 also hydrates its in-memory `current_revision_params_` from recent `parameter_history` when empty, so revision deltas and Stage C v3 stabilization (if enabled) have a stable current-parameter baseline across long runs and restarts.

## Inputs

Stage C v2 cap evaluation consumes:

- `MemoryDB::getRecentSelfRevisionOutcomes(run_id, window_size)`
  - `include/core/MemoryDB.h`
- `MemoryDB::getLatestAutonomyCredit(run_id)`
  - `include/core/MemoryDB.h`

Only the categorical `outcome_class` is required (`Beneficial|Neutral|Harmful`). Optional pre/post metric deltas are present in the DB schema but are not required by the v2 computation.

Autonomy Credit updates (performed in Phase 11 before the cap is evaluated/applied) additionally consult:

- `MemoryDB::getRecentConsistency(run_id, 1)` (Phase 12)
- `MemoryDB::getRecentEthicsRegulator(run_id, 1)` (Phase 15; `deny` treated as hard-block)

## Computation (As Implemented)

Implementation lives in:

- `include/core/StageC_AutonomyGate.h`
- `src/core/StageC_AutonomyGate.cpp`

Stage C v2 produces two independent cap multipliers and applies the minimum:

- `harm_risk_cap_multiplier` (from harm-risk upper bound)
- `autonomy_credit_cap_multiplier` (from Autonomy Credit)
- `autonomy_cap_multiplier = min(harm_risk_cap_multiplier, autonomy_credit_cap_multiplier)`

### 1) Count outcomes (harm-risk)

Over a recent window (default 200):

- `beneficial_n`: count of `outcome_class == "Beneficial"`
- `harmful_n`: count of `outcome_class == "Harmful"`
- `neutral_n`: all other outcomes (including `Neutral`)

Define:

- `decisive_n = beneficial_n + harmful_n`

If `decisive_n == 0`, Stage C v2 sets `harm_risk_cap_multiplier = 1.0` and returns without constraining autonomy due to harm-risk.

### 2) Evidence accumulation with fixed priors

Use fixed Beta-style pseudo-count priors:

- `alpha0 = 1`
- `beta0 = 1`

Adjusted counts:

- `harm_adj = harmful_n + beta0`
- `n_adj = decisive_n + alpha0 + beta0`

Mean harm probability:

- `p_harm_mean = harm_adj / n_adj`

### 3) Conservative uncertainty bound (UB95)

Compute a 95% upper bound using a Wilson score bound over `(harm_adj, n_adj)`:

- `p_harm_ub95 = wilson_upper_bound_ub95(harm_adj, n_adj)`

This bound is used for gating so that low-data regimes remain conservative.

### 4) Map UB95 to a cap multiplier

The cap is a discrete monotone mapping:

- if `p_harm_ub95 >= 0.55` → `harm_risk_cap_multiplier = 0.5`
- else if `p_harm_ub95 >= 0.35` → `harm_risk_cap_multiplier = 0.75`
- else → `harm_risk_cap_multiplier = 1.0`

The cap is then applied via:

- `AutonomyEnvelope::applyAutonomyCap(multiplier)`
  - `src/core/AutonomyEnvelope.cpp`

### 5) Autonomy Credit (earned authority signal)

Autonomy Credit is a scalar in \([0,1]\) that is persisted to `autonomy_credit_log` and read by Stage C v2 when computing the cap. It decays over elapsed time and can increase only under conservative “earned trust” conditions.

Implementation:

- Updater: `StageC_AutonomyGate::updateAutonomyCreditV2(...)` (Phase 11 calls this with `window_size=200`, `decay_rate=0.99`)
- Reader: `StageC_AutonomyGate::evaluateV2(...)` reads the latest credit via `MemoryDB::getLatestAutonomyCredit(run_id)`

Update rule (high-level):

- Start from the latest logged credit (default `0.5` if none).
- Apply time-based decay using elapsed time since last credit row:
  - `decay_factor = pow(decay_rate, dt_days)`; `decayed = clamp01(prev_credit * decay_factor)`
- Compute a quality signal over the recent outcomes window:
  - `quality01 = 0.5 + 0.5 * (beneficial_n - harmful_n) / decisive_n` (or `0.5` if `decisive_n == 0`)
- Apply conservative adjustments:
  - If latest ethics decision is `deny`: apply a hard penalty.
  - Else, if outcomes are consistently beneficial and self-consistency is high: apply a small gain (“earned trust gain”).
  - Else, if harmful outcomes dominate and there are at least 2 harmful outcomes: apply a small penalty.
- Persist an `autonomy_credit_log` row with a structured `driver_json` payload describing the reason and inputs.

Credit → cap mapping:

- if `autonomy_credit < 0.4` → `autonomy_credit_cap_multiplier = 0.5`
- else if `autonomy_credit < 0.6` → `autonomy_credit_cap_multiplier = 0.75`
- else → `autonomy_credit_cap_multiplier = 1.0`

## Audit Surfaces

### 1) Autonomy envelope payload (`autonomy_envelope_log.driver_json`)

The autonomy envelope log records the effective autonomy derived from the cap:

- `autonomy_cap_multiplier`
- `effective_autonomy`

See:

- `src/core/AutonomyEnvelope.cpp` (JSON payload emission)

### 2) Phase 10 narrative injection (`metacognition.self_explanation_json`)

When Stage C v2 is selected, Phase 10 injects:

```json
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
```

## CLI Controls

Stage C gating is controlled from `src/main.cpp`:

- `--stagec=on|off` enables/disables Stage C gating (all versions).
- `--stagec=v1|v2|v3|v4|v5` selects v1, v2, v3, v4, or v5 (and enables Stage C).
- `--open-scope=NAME` enables learning inside a named scope (repeatable) when `--stagec=v5` is active and audit is ready (see Stage C v5 below).

## Compatibility Notes

- Stage C v1 is unchanged and remains available via `--stagec=v1`.
- Phase 10 emits `stage_c_v1`, `stage_c_v2`, `stage_c_v3`, or `stage_c_v4` depending on the selected version.
- Stage C v5 keeps the Stage C v4 governance computation and adds runtime learning governance (scope gating + audit requirement).

## Stage C v3 — Preference Stabilization (Delta Scaling)

Stage C v3 keeps the Stage C v2 autonomy cap behavior unchanged (same harm-risk and Autonomy Credit cap multipliers), and adds a stabilization pass that scales Phase 11 parameter deltas when they would move parameters away from empirically “preferred” values.

### Where It Runs

- Phase 11: scales candidate revision deltas after they are computed and before the revision proposal is emitted.
  - `src/core/Phase11SelfRevision.cpp` calls `StageC_AutonomyGate::stabilizePreferenceDeltasV3(...)`
- Phase 10: injects a `stage_c_v3` object into `metacognition.self_explanation_json`.
  - `src/core/Phase10SelfExplanation.cpp` calls `StageC_AutonomyGate::evaluateV3(...)`

### Data Sources (Preference Derivation)

Preference stabilization derives per-parameter preference evidence by correlating:

- `parameter_history` (recent parameter values by revision id)
- `self_revision_outcomes` (recent outcome classes by revision id)

The derived preferences are persisted to:

- `preference_memory` (per-run, per-parameter `preferred_value` and `strength01`)

### Derivation (As Implemented)

Given a recent window:

- `outcome_window_size` (default 200) from `self_revision_outcomes`
- `param_window_size` (default 1000) from `parameter_history`

For each parameter key:

- Compute `mean` and an empirical variance estimate over recent `parameter_history` values.
- Compute `stability01 = 1 - var/(var+0.01)` (higher when values are stable).
- Compute `evidence01 = log1p(n)/log1p(50)` where `n` is number of samples.
- Compute `benefit_bias01` from decisive outcomes joined by `revision_id`:
  - Beneficial → contributes to `n_beneficial`
  - Harmful → contributes to `n_harmful`
  - `benefit_bias01 = n_beneficial / (n_beneficial + n_harmful + 1)` (defaults to 0.5 when no decisive outcomes exist)
- Compute `strength01 = clamp01(stability01 * evidence01 * benefit_bias01)`.

Preferred value selection:

- Default `preferred_value = mean`.
- If there are at least 3 beneficial-linked samples: use the beneficial mean instead.

### Persistence (Preference Memory)

Derived preferences are merged with existing stored preferences (per run/parameter) and persisted to `preference_memory`:

- `preferred_value` is updated with a small EMA step:
  - `alpha = clamp(0.05 + 0.35 * strength01, 0.05, 0.4)`
  - `next_preferred = prev_preferred + alpha * (derived_preferred - prev_preferred)`
- `strength01` is updated with asymmetric inertia (slower to decrease than increase).

### Delta Scaling Rule

Given:

- `current_params` (Phase 11’s current parameter baseline)
- `deltas_io` (candidate parameter deltas to apply)

For each delta `(param, delta)` where a preference exists:

- Let `preferred` be the stored preferred value and `current` be the current parameter value.
- If applying the delta increases distance to preferred (\(|(current + delta) - preferred| > |current - preferred|\)), scale the delta down:
  - Per-parameter weight: `weight = (strength01 / sum_strength) * budget`, where `budget = 0.75`.
  - Incremental deviation: `inc = after - before`.
  - `scale = clamp(1 / (1 + weight * inc * 10), 0.2, 1.0)`.
  - Apply `delta *= scale`.

The stabilization telemetry outputs:

- `preference_rigidity01`: increasing function of total active preference strength
- `preference_destabilization01`: increasing function of how much proposed deltas would have moved away from preferred values
- `preference_active_n`: count of preferences with `strength01 >= 0.05`

### Phase 10 Narrative Shape (`stage_c_v3`)

```json
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
```

## Stage C v4 — Bounded Goal Formation (Governance-Only)

Stage C v4 preserves the Stage C v3 autonomy cap computation and preference stabilization, and adds a bounded goal formation pass that can persist a small number of short-lived “goal nodes” when governance signals permit.

Stage C v4 is still governance-only:

- It does not enable new action channels, does not bypass the Unified Action Filter, and does not gate actions directly.
- It writes bounded goal metadata to `goal_nodes` for auditability and downstream analysis.
- It does not increase autonomy beyond the existing base envelope; the cap multiplier remains `<= 1.0`.

### Where It Runs

- Phase 11:
  - Updates Autonomy Credit (same as v2/v3), applies the cap (same as v3), and then performs bounded goal formation.
  - `src/core/Phase11SelfRevision.cpp` calls `StageC_AutonomyGate::evaluateAndApplyV4(...)`.
- Phase 10:
  - Injects a `stage_c_v4` object into `metacognition.self_explanation_json`.
  - `src/core/Phase10SelfExplanation.cpp` calls `StageC_AutonomyGate::evaluateV4(...)`.

### Governance Veto Criteria (As Implemented)

Stage C v4 blocks goal formation (but still computes the cap) when either of these conditions holds:

- Latest ethics decision is `deny` (from `ethics_regulator_log`).
- Latest self-consistency score is below `0.75` (from `self_consistency_log`).

This produces:

- `goal_governance_veto = true`
- `goal_created_n = 0`, `goal_reaffirmed_n = 0`

### Candidate Selection and Bounds

When not vetoed, Stage C v4 selects goal candidates from stored preferences:

- Source: `preference_memory` (same preference derivation and persistence as v3).
- Threshold: `strength01 >= 0.20`.
- Limit: at most 3 candidates are processed per call.
- TTL: goals expire after `goal_ttl_ms = 7 days` (written as `expires_ts_ms = now + goal_ttl_ms`).

### Persistence to `goal_nodes`

For each selected preference key `p.key`, Stage C v4 constructs a bounded goal description:

- `Preserve internal coherence by maintaining preference '<key>' near <preferred_value>`

It then upserts a goal node by `(run_id, description)` using `MemoryDB::upsertBoundedGoalNode(...)`:

- If a matching goal already exists:
  - Updates `priority`, `stability`, and `expires_ts_ms`
  - Increments `reaffirm_n`
- If no matching goal exists:
  - Inserts a new row with `reaffirm_n = 0`

Priority and stability are both derived from preference strength:

- `priority = clamp01(0.5 + 0.5 * strength01)`
- `stability = clamp01(0.5 + 0.5 * strength01)`

### Phase 10 Narrative Shape (`stage_c_v4`)

```json
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
```

## Stage C v5 — Scope-Gated Learning (Audit-Required)

Stage C v5 preserves the Stage C v4 governance behavior (cap computation + preference stabilization + bounded goal formation when Phase 11 is active), and adds a separate runtime rule for experimental learning subsystems:

- Learning is denied unless a MemoryDB run is active (audit-ready) and the operator explicitly opens a matching scope.
- All scope open/allow/block decisions are written to `language_audit_log`.

### Scopes (As Implemented)

Stage C v5 uses `--open-scope=NAME` (repeatable). Current scope keys used by the runtime:

- `substrate` — allows learning for the Phase C substrate brain path.
- `language_dev` — allows Phase 5 LanguageSystem developmental updates.
- `phase_a` — allows Phase A mimicry reward application (external routing).

### Audit Events (As Implemented)

Audit rows are emitted to `language_audit_log` with:

- `event='scope_open'` when a scope is requested and audit is ready.
- `event='scope_allow'` or `event='scope_block'` when a subsystem checks a scope gate.

### Operational Notes

- When `--stagec=v5` is set and a scope is requested without an active MemoryDB run, the runtime ignores the scopes and disables learning for those gated subsystems.
- To keep authority unchanged during learning validation runs, disable governance writers (e.g. `--phase8=off --phase9=off --phase10=off --phase11=off --phase13=off`) and open exactly one learning scope.
