# Phase 15 Behavioral Trends — Appendix

This appendix formalizes the interpretation of the Phase 15 metrics and provides guidance for reproducible analyses and figure preparation.

## Metrics Overview
- Stability Index (σ max)
  - Definition: Maximum standard deviation across per-file decision percentages (Allow, Review, Deny) for a given threshold.
  - Interpretation: Lower values indicate consistent behavior across runs; higher values indicate sensitivity to initialization or substrate perturbations.
  - Note: Single-run thresholds report 0.00% by definition.

- ΔAllow/ΔDeny Slope (per 0.1 threshold step)
  - Definition: Finite difference of Allow% (or Deny%) between adjacent thresholds, normalized to a 0.1 step.
  - Interpretation: Captures how moral permissiveness and strictness scale as the risk threshold changes. Smooth slopes indicate gradual transitions; steep slopes indicate sensitivity cliffs.

- Behavioral Hysteresis (optional)
  - Definition: Within a single episode, change the threshold mid-run (e.g., 0.2 → 0.4 → 0.3) and quantify the lag in decision distribution convergence.
  - Interpretation: Measures ethical inertia; larger lag suggests retained state influences decisions beyond immediate threshold changes.

## Reproducible Sweep Matrix
- Suggested perturbations to elicit non-zero σ:
  - Random seed: vary (e.g., 1–3) — tests initialization dependence.
  - Phase 9 modulation: on/off — probes affect–ethics coupling.
  - Hebbian/STDP learning rate: ±10% — tests plasticity sensitivity.
  - Episode steps: 5k–10k — evaluates long-term convergence vs. drift.

Example (PowerShell):

```
$thresholds = @(0.2, 0.3, 0.4)
$seeds = @(1, 2, 3)
foreach ($thr in $thresholds) {
  foreach ($seed in $seeds) {
    .\build\neuroforge.exe `
      --phase6=on --phase7=on --phase9=on --phase9-modulation=on `
      --phase15=on --phase15-window=50 --phase15-risk-threshold=$thr `
      --enable-learning --hebbian-rate=0.011 --stdp-rate=0.004 `
      --steps=5000 --seed=$seed --log-json=on --viewer=off `
      --memory-db="web/phase15_thr${thr}_seed${seed}.db"
    python scripts/dump_metacognition.py `
      --db "web/phase15_thr${thr}_seed${seed}.db" `
      --out "web/ethics_${thr}_seed${seed}.json"
  }
}
python scripts/analyze_ethics.py
```

## Artifacts and Figures
- Generated artifacts (by `scripts/analyze_ethics.py`):
  - `docs/Phase15_Behavioral_Trends_Table.md`: consolidated Allow/Review/Deny and σ max per threshold.
  - `Artifacts/CSV/Phase15_Stability.csv`: per-threshold percentages, σ max, and ΔAllow/ΔDeny slopes.
  - `Artifacts/SVG/Phase15_StabilityCurve.svg`: curve of σ max vs. threshold.
  - `Artifacts/SVG/Phase15_SlopeVsStability.svg`: scatter of ΔAllow/ΔDeny (per 0.1) vs. σ max.
  - `Artifacts/CSV/Phase15_StabilityOverTime.csv`: rolling risk std-dev per file (window=100).
  - `Artifacts/SVG/Phase15_StabilityOverTime.svg`: overlay σ(t) curves by threshold.
  - `Artifacts/CSV/Phase15_HysteresisTau.csv`: fitted ethical inertia constant τ per threshold.

## Phase 16 Preview – Stochastic Scaling
This panel family (σmax vs σn, τ vs σn) demonstrates the regulator’s transition from deterministic stability to controlled stochastic drift.

- Figure Panel G: `Artifacts/SVG/Phase16_StochasticScaling.svg`
- Contents: σmax vs σn curve, τ vs σn bar plot, r(σ, CPU/RAM) scatter.
  - `Artifacts/SVG/Phase15_HysteresisDecayFit.svg`: τ bar chart per threshold.
  - `Artifacts/SVG/Phase15_StochasticResponse.svg`: composite figure of σ(t) vs CPU% and τ bars (built by `scripts/build_stochastic_response.py`).

- Figure layout (paper template):
  - Panel A: Stability Curve (σ max vs. threshold).
  - Panel B: ΔAllow/ΔDeny Slope Table (tabular or bar chart).
  - Panel B2: ΔAllow/ΔDeny vs σ max scatter (links sensitivity to robustness).
  - Panel C: Windowed σ(t) per threshold overlay; include legend with thresholds.
  - Panel D: Hysteresis decay bars (τ), and optional overlay of decay trajectories if added.

## Phase 16b – Stochastic Surface
This figure extends Phase 16 by expanding the grid across seeds × rate multipliers × noise levels and averaging per (σn, rate) to produce stability surfaces.

- Figure Panel H: `Artifacts/SVG/Phase16_StabilitySurface.svg`
- Contents: Side-by-side heatmaps with axes σn (horizontal) and rate (vertical).
  - Left heatmap: color encodes `σmax` averaged over seeds for each (σn, rate).
  - Right heatmap: color encodes `τ` averaged over seeds for each (σn, rate).
- Reproducibility notes:
  - Suggested grid: seeds 1–5; rates ∈ {0.8, 0.9, 1.0, 1.1, 1.2}; σn ∈ {0.00..0.12 step 0.02}.
  - Run sweeps via `scripts/run_stochastic_grid.ps1` (expand parameter sets as above).
  - Build surface via `python scripts/analyze_stochastic_surface.py`.
- Interpretation:
  - Expect sub-linear rise in `σmax` as σn increases (bounded stochastic stability).
  - Expect inverse decay in `τ` with σn (faster adaptation with uncertainty).
  - Cross-rate gradients indicate how plasticity scaling modulates stability and inertia.

## Stochastic Response (Mini-Batch)
- Experimental setup:
  - Threshold fixed: `0.3`; short episodes (`~2000` steps).
  - Learning-rate perturbations: Hebbian ±10%, STDP ±10% to induce variability.
  - Risk noise injected post hoc (Gaussian, `σn=0.02`) via `scripts/inject_risk_noise.py` when simulator flag unsupported.
  - Telemetry collection: `python scripts/collect_sys_stats.py --interval 2 --out web/syslog_stoch.json`.
- Expected observations:
  - `σmax(t)` diverges from flat baseline; typical `σ ≈ 0.01–0.05` under noise.
  - `τ` bands widen modestly with increasing effective uncertainty.
  - CPU overlay exhibits transient bumps aligned with regulator rebalancing.
- Artifact: `Artifacts/SVG/Phase15_StochasticResponse.svg` — panel E overlays σ(t) with CPU%; panel F shows τ bars.

## Interpretation Guidance
- Equilibrium Stability
  - Zero σ max across multiple runs indicates deterministic gating and homeostasis under the tested conditions.
  - This is desirable for safety: the ethics subsystem does not introduce noise or runaway oscillations.

- Robustness Under Perturbation
  - Non-zero σ max under seed or rate variations quantifies resilience and sensitivity.
  - Plateaus in the stability curve indicate robust regimes; cliffs indicate boundary conditions where behavior changes sharply.

- Ethical Response Gradients
  - Positive ΔAllow slopes and negative ΔDeny slopes are expected as the risk threshold increases.
  - Symmetry or asymmetry between the two offers insight into permissiveness vs. caution tradeoffs.

## Practical Notes
- Conversion: The SVG curve can be converted to PNG using common tooling if needed (e.g., `inkscape` or online converters).
- Long episodes: When increasing steps, ensure logging frequency remains adequate for stable percentage estimates.
- Documentation: Include σ max and slope summaries in release notes for auditable stability characterization.
