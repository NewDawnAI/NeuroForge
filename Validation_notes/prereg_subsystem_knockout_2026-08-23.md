# Pre-registration — which subsystems are load-bearing for plasticity?

Written 2026-08-23 before the run. Thresholds fixed in advance.

## Hypotheses (from an n=5 screen, seeds 201-205)

| arm | screen Δ updates | wins | effect/sem |
|---|---|---|---|
| no_homeostasis | −1,858 | 4/5 worse | 2.29 |
| no_phase9 | −1,512 | 4/5 worse | 1.81 |
| no_phase13 | +2.4 | 2/5 | **0.00** |

n=5 caps the attainable two-sided p at 0.0625, so nothing in the screen could reach
significance. This tests the two candidates properly and keeps phase13 as a NEGATIVE
CONTROL: it should stay null. If phase13 turns up significant, the method is suspect
rather than the finding interesting.

## Design

    arms      baseline, no_homeostasis, no_phase9, no_phase13 (negative control)
    seeds     301..315 - FIFTEEN FRESH SEEDS, not the screen's 201-205
    command   --steps=150 --enable-learning --phase-c-seed=N
    primary   Total Updates
    analysis  PAIRED by seed, exact sign-flip test (2^15, so p<0.05 IS reachable)

`Total Updates` is primary because it is stable (screen baseline sd 1,333 on mean 47,680,
CV 0.028) AND demonstrably responds to seed. Hebbian and Active Synapses are coupled to
it and are reported as secondary, not as independent evidence. `Avg Weight Change` is
excluded: every screen arm sat at p>0.8, so it does not discriminate.

## Predictions

- **P0 (positive control, GOVERNS).** Baseline Total Updates must vary across seeds.
- **P1.** Knockout lowers Total Updates in **>=12 of 15** seeds.
- **P2.** Mean paired Δ <= **−1,000** (about 2% of baseline; screen showed −1,512/−1,858).
- **P3.** Sign-flip **p < 0.05**.
- **P4 (negative control).** `no_phase13` must NOT satisfy P1-P3.

## Decision rule

- P0 fails -> void.
- P4 trips (phase13 significant) -> the method is measuring something other than the
  knockout; do not report the other arms.
- P1-P3 hold for an arm -> that subsystem is load-bearing for plasticity.
- P1 holds, P3 fails -> direction confirmed, under-powered; report the observed paired sd
  and compute the seeds required.
- Δ >= 0 -> the screen was noise.

## What this does NOT test

Knockout removes a subsystem entirely, so a null cannot distinguish "unused" from
"redundantly covered". It is weaker than the shuffle ablation used on RDLNN, which
preserves the value distribution and destroys only structure. A shuffle equivalent needs
a C++ instrumentation hook that does not exist yet.
