# Stage C v1 — Governance-Only Autonomy Gating (from Self-Revision Outcomes)

_Updated: 2025-12-19 • Scope: Phase 10/11/13 integration_

Stage C v1 is a governance-only mechanism that can **reduce** the system’s effective autonomy when recent self-revision outcomes are neutral or harmful. It does not grant additional capabilities, does not approve revisions, and does not directly gate actions.

## What It Does
- Computes a revision reputation score from recent `self_revision_outcomes`.
- Maps that reputation to an autonomy cap multiplier in \[0.5, 1.0\].
- Applies the multiplier to the autonomy envelope, producing:
  - `effective_autonomy = autonomy_score * autonomy_cap_multiplier`

This cap influences exploration modulation in Phase 6 by reducing the autonomy signal used for option ranking perturbations.

## Data Sources
- `self_revision_outcomes` (Phase 11, evaluation-only): per-revision pre/post metrics and an `outcome_class`.
- `autonomy_envelope_log` (Phase 13): includes `autonomy_cap_multiplier` and `effective_autonomy` inside `driver_json`.
- `metacognition.self_explanation_json` (Phase 10): may embed `stage_c_v1` to make the cap auditable in narratives.

## Outcome Classes → Reputation
Stage C v1 uses `outcome_class` values:
- `Beneficial` → +1
- `Harmful` → −1
- Anything else (including `Neutral`) → 0

Given a window of \(N\) recent outcomes:
- `mean = sum / N`
- `revision_reputation = clamp(0.5 + 0.5 * mean, 0.0, 1.0)`

## Reputation → Cap Multiplier
The cap is a coarse, monotone mapping:
- `revision_reputation < 0.4` → `autonomy_cap_multiplier = 0.5`
- `revision_reputation < 0.6` → `autonomy_cap_multiplier = 0.75`
- Otherwise → `autonomy_cap_multiplier = 1.0`

## Where It Runs
- Phase 11 applies the cap during self-revision execution:
  - `src/core/Phase11SelfRevision.cpp` calls `StageC_AutonomyGate::evaluateAndApply(..., window_size=20)`.
- Phase 10 embeds `stage_c_v1` into the self-explanation narrative:
  - `src/core/Phase10SelfExplanation.cpp` computes the same reputation and cap over the last 20 outcomes and injects them into `metacognition.self_explanation_json`.

## Audit Surfaces

### 1) Narrative injection (`metacognition.self_explanation_json`)
Example shape:
```json
{
  "summary": "Trust unchanged due to recent prediction outcomes.",
  "drivers": { "avg_abs_error": 0.0, "confidence_bias": 0.0 },
  "context": "post-resolution attribution",
  "stage_c_v1": {
    "revision_reputation": 0.50,
    "autonomy_cap_multiplier": 0.75,
    "window_n": 20,
    "note": "Autonomy constrained due to recent neutral/harmful self-revision outcomes"
  }
}
```

### 2) Autonomy envelope log (`autonomy_envelope_log.driver_json`)
The autonomy envelope log includes:
- `autonomy_score` (base)
- `autonomy_cap_multiplier` (cap)
- `effective_autonomy` (product)

This makes it possible to compare base vs. capped autonomy over time.

## Operational Notes
- Window size is currently fixed at 20 in Phase 10 and Phase 11.
- If there are no outcomes in the window, Stage C v1 does not apply a cap (`autonomy_cap_multiplier = 1.0`).
- Stage C v1 is designed to be conservative: it clamps autonomy down, never up.

