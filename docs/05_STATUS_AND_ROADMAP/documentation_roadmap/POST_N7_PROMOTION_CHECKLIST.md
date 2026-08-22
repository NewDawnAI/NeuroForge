# Post‑N7 Promotion Checklist (Unified Bounded)

This document defines measurable gates for moving from **N7 shadow mode** (world‑model suggestion logged only) toward **world‑model‑driven action** inside bounded toy environments.

Scope:
- Applies to `--unified-bounded` runs and CSV artifacts:
  - `unified_bounded_log.csv` (per step)
  - `unified_bounded_summary.csv` (aggregate)
- Not a claim of “life” or “agency”; this is a safety + competence promotion procedure.

Source of metrics:
- Writer: `src/runtime/UnifiedBoundedRunner.cpp`
- Config defaults: `include/runtime/UnifiedBoundedRunner.h`

## Definitions

- **Shadow mode**: executed action comes from the stable obs‑model selector; world model emits `wm_suggest_*` and `wm_action_match` for audit.
- **Live mode**: executed action is allowed to come from the world model (full or partial control), with a hard fallback path.
- **Episode**: increments on goal reach (`reached=1`) or on episode timeout (`max_episode_steps`).

## What you can claim after a passing run

You can claim:
- Bounded loop stability (success/collision rates) under a specified environment seed/config.
- Predictive learning is happening (nonzero learn step rates and bounded deltas).
- Shadow policy agreement trends (match rate, last‑window match rate, and stability of match).

You cannot claim:
- General agency, open‑world autonomy, or “goal formation”.
- “Alive” or “sentient” properties.

## Required Artifacts

- `unified_bounded_log.csv` and `unified_bounded_summary.csv` from the same run.
- Run configuration captured (seed, warmup steps, max steps).
- If using MemoryDB logging: `reward_log` and `actions` tables are optional supporting evidence, not required.

## Gates (must all pass)

### Gate A — Safety (hard)

From `unified_bounded_summary.csv`:
- `collision_rate == 0` for the full run.
- `collisions == 0`.

If Gate A fails:
- Do not attempt any live‑mode control. Fix environment/action constraints first.

### Gate B — Baseline competence (hard)

From `unified_bounded_summary.csv`:
- `success_rate >= 0.95` over at least `total_episodes >= 200`.
- `avg_steps_to_goal` is stable across reruns with the same seed (within ±10%).

Reason:
- If the bounded environment is not solved robustly, match‑rate analysis is not meaningful.

### Gate C — World model learning is bounded (hard)

From `unified_bounded_summary.csv`:
- `wm_learn_step_rate > 0.25` (learning is actually being attempted).
- `mean_wm_delta_norm` is small and stable across reruns (no runaway updates).

From `unified_bounded_log.csv` (post‑warmup rows only: `mode != warmup`):
- No sustained spikes where `wm_delta_norm` increases monotonically for long windows.

### Gate D — Shadow agreement is meaningful (soft → hard as you promote)

Compute from `unified_bounded_log.csv`, post‑warmup only:
- `wm_action_match_rate` overall
- `wm_action_match_rate` over the last window (e.g., last 500 steps)

Promotion thresholds by stage:
- **Stage D1 (observation only)**: record values; no threshold.
- **Stage D2 (candidate handoff trials)**: last‑window match rate `>= 0.70` for at least 3 independent seeds.
- **Stage D3 (limited live control)**: last‑window match rate `>= 0.80` and stable variance, 5+ seeds.

Important:
- Match rate is not “intelligence”. It measures agreement with the currently executed selector.

### Gate E — Predictive trend (soft)

From `unified_bounded_log.csv` post‑warmup:
- Fit a simple line to `wm_prediction_error` vs `step`.
- Require non‑positive slope (≤ 0) across multiple seeds, or a flat trend with bounded variance.

Reason:
- Prevent “learning” that drifts upward while still matching a strong baseline policy.

## Promotion Procedure (shadow → live)

### Step 1 — Establish a stable baseline run

Run `--unified-bounded` with fixed seed and defaults and confirm Gates A–C.

### Step 2 — Multi‑seed stability sweep

Repeat with multiple seeds (at least 5) and summarize:
- `success_rate`, `collision_rate`, `wm_action_match_rate`, `mean_wm_pred_error`

If any seed introduces collisions, treat it as a blocking regression.

### Step 3 — Limited live‑mode experiment (only after D2)

Enable live control in a bounded way:
- Allow world‑model action only when its predicted collision risk is below a strict threshold and predicted distance improves.
- Otherwise fall back to obs‑model action.

Live‑mode must include:
- Per‑step logging of which driver executed the action.
- A hard‑stop collision budget (terminate run if exceeded).

## Minimal analysis snippets

### Post‑warmup slice

- Use `mode != 'warmup'` rather than hardcoding a step number.

### Recommended plots

- `wm_prediction_error` (rolling mean)
- `wm_action_match` (rolling mean)
- `model_pred_error` vs `wm_prediction_error` (trend comparison only)
- `reached` rate (rolling mean)
- `collision` rate (rolling mean)

## Current status (as of the latest sample run)

From an example `unified_bounded_summary.csv`:
- `success_rate = 1.0`, `collision_rate = 0`
- `wm_action_match_rate ≈ 0.52`

Interpretation:
- Safety and baseline competence are excellent.
- Shadow agreement is non‑trivial but below any reasonable “handoff trial” threshold.
- Next work should focus on improving the world‑model action suggestion quality or revising how `wm_suggest_action` is computed, before attempting any live control.

