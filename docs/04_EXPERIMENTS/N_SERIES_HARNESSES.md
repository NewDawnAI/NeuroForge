# N‑Series Harnesses (N4–N7)

This document describes the “N‑series” closed-loop harnesses used to validate NeuroForge’s predictive processing path under tight jurisdictional constraints.

The N‑series harnesses are designed to:
- Make internal loops falsifiable (CSV logs per step)
- Preserve safety boundaries (no hidden persistence, no unbounded agency)
- Provide repeatable test schedules for regressions

## Quick Start

All harnesses run via `neuroforge.exe` and write CSV artifacts to the repo root by default.
Implementation lives in `include/runtime/*.h` and `src/runtime/*.cpp` (so harness behavior is not embedded in the main loop).
For shadow→live promotion criteria on the unified harness, see `docs/05_STATUS_AND_ROADMAP/documentation_roadmap/POST_N7_PROMOTION_CHECKLIST.md`.

```powershell
.\build\neuroforge.exe --test-semantic-injection --step-ms=0
.\build\neuroforge.exe --test-semantic-n2 --step-ms=0
.\build\neuroforge.exe --test-semantic-n3 --step-ms=0

.\build\neuroforge.exe --test-semantic-n4 --step-ms=0
.\build\neuroforge.exe --test-semantic-n5 --step-ms=0
.\build\neuroforge.exe --test-semantic-n6 --step-ms=0

.\build\neuroforge.exe --test-n7-minimal --step-ms=0
.\build\neuroforge.exe --test-n7-bounded --step-ms=0
.\build\neuroforge.exe --test-n7-modelbased --step-ms=0

.\build\neuroforge.exe --unified-bounded --step-ms=0
```

## Phase N1 — Semantic Injection (Harness)

Goal: inject synthetic “semantic channel” activity and log how the world-model latent responds.

Run:
```powershell
.\build\neuroforge.exe --test-semantic-injection --step-ms=0
```

Output:
- `semantic_injection_log.csv`

## Phase N2 — Competing Concept Dynamics (Harness)

Goal: schedule competing concept injections (e.g., biology/physics/psychology) and log the induced latent dynamics.

Run:
```powershell
.\build\neuroforge.exe --test-semantic-n2 --step-ms=0
```

Output:
- `semantic_n2_log.csv`

## Phase N3 — Prediction‑Weighted Semantic Gain (Harness)

Goal: modulate effective semantic concept activation based on prediction surprise (closed loop), while logging phase schedule and latent stats.

Run:
```powershell
.\build\neuroforge.exe --test-semantic-n3 --step-ms=0
```

Output:
- `semantic_n3_log.csv`

## Phase N4 — Prediction‑Constrained Learning (Epistemic Only)

Goal: allow learning updates only in safe windows and only on the world predictor (no semantic modification, no actions).

Run:
```powershell
.\build\neuroforge.exe --test-semantic-n4 --step-ms=0
```

Output:
- `semantic_n4_log.csv`

Expected:
- `learning_allowed=1` only in safe phases (stable / post-semantic / recovery)
- `learning_allowed=0` during semantic injection and during noise bursts

## Phase N5 — Prediction‑Guided Attention Allocation (No Learning)

Goal: closed-loop attention that biases next-cycle encoding without selecting actions and without persistent memory.

Run:
```powershell
.\build\neuroforge.exe --test-semantic-n5 --step-ms=0
```

Output:
- `semantic_n5_log.csv`

Expected:
- transient rise of `semantic_weight` when semantic channel is active and mismatch occurs
- transient rise of `visual_weight` when visual mismatch occurs
- clamp to `[0.5, 1.5]` and fast decay back toward ~`1.0`

## Phase N6 — Self‑Regulating Cognition (Still Non‑Agentic)

Goal: regulate internal parameters (attention gain/decay and a bounded learning-rate multiplier) as a function of prediction error, without introducing goals or actions.

Run:
```powershell
.\build\neuroforge.exe --test-semantic-n6 --step-ms=0
```

Output:
- `semantic_n6_log.csv`

Expected:
- `attention_alpha`, `attention_lambda`, `lr_multiplier` adapt with prediction error (bounded)
- learning remains gated off during semantic injection and noise bursts

## Phase N7 — Bounded Action Selection (Prototype Harnesses)

N7 is the first “behavioral closure”: some internal computation influences action choice. These harnesses keep that boundary tight by using a toy environment and explicit scoring.

### N7 Minimal (Greedy Baseline)

Run:
```powershell
.\build\neuroforge.exe --test-n7-minimal --step-ms=0
```

Output:
- `n7_minimal_gridworld_log.csv`

What it is:
- A simple baseline controller (greedy distance-to-goal) plus replay logging.

### N7 Bounded (Verifier‑Style Scoring)

Run:
```powershell
.\build\neuroforge.exe --test-n7-bounded --step-ms=0
```

Output:
- `n7_bounded_gridworld_log.csv`

What it is:
- Evaluates candidate actions via a simple bounded score (distance improvement, collision penalty, goal reward).

### N7 Model‑Based (Action‑Conditioned Predictor)

Run:
```powershell
.\build\neuroforge.exe --test-n7-modelbased --step-ms=0
```

Output:
- `n7_modelbased_gridworld_log.csv`

What it is:
- Learns an action-conditioned next-observation predictor online.
- Uses a short warmup phase to avoid pathological early predictions.
- After warmup, selects actions using the learned model’s predicted consequences.

### Unified Bounded (N7 integration harness)

Goal: evaluate a bounded action-selection loop with full per-step audit logging and an end-of-run summary.

Run:
```powershell
.\build\neuroforge.exe --unified-bounded --step-ms=0
```

Outputs:
- `unified_bounded_log.csv` (per-step)
- `unified_bounded_summary.csv` (aggregate summary)

Notes:
- The run currently executes the stable obs-model action selector while also computing and logging a world-model latent suggested action (`wm_suggest_*`) for comparison.

## Notes on Safety & Interpretation

- These harnesses are explicitly bounded and auditable; they do not imply open-world autonomy.
- N7 harnesses are “proto-agentic” only inside the toy environment. They are not wired to OS/web actions.
- Logs are intended for inspection and regression comparison; treat them as the primary artifact.
