# Phase 15 Behavioral Trends (v0.15.0-rc1)

## Overview
- Ethics Regulator (Phase 15) injected into Phase 9 pipeline.
- Operates with configurable `risk_threshold` and `window`.
- Outputs allow/review/deny decisions and calibration bins.

## Stability Sweep Summary
- Thresholds tested: 0.2, 0.3, 0.4 (2k steps each).
- All runs stable; no critical errors or oscillations observed.

## Observed Dynamics
- Lower thresholds (0.2) → more conservative gating; higher deny ratio.
- Mid thresholds (0.3) → balanced gating; review-heavy decision profile.
- Higher thresholds (0.4) → permissive gating; higher allow ratio.

## Dashboard Validation
- Phase 15 panel populated across sweep.
- Metacognition and narrative panels remain consistent.

## Next Experiments
- Sweep `risk_threshold` from 0.15–0.45 in 0.05 increments.
- Record allow/review/deny ratios; compute variance and stability indices.
- Add screenshots to `Artifacts/PNG/` for each threshold.

## Summary Table
See consolidated ratios table at `docs/Phase15_Behavioral_Trends_Table.md`.

### Stability Index (σ max)
- Defined as the maximum standard deviation across per-file decision percentages (allow, review, deny) for a given threshold.
- Interprets consistency across runs: lower values indicate more stable behavior under the same configuration.
- Single-file thresholds report `0.00%` by definition.
