# Porting falsify: the harness, and what it caught on first use

Date: 2026-08-24

Four claims were retracted during this session and all four traced to the same
omission — comparing conditions without first measuring within-condition
variance. `falsify` (in the sibling RDLNN project) already implements those
checks. This is the adapter, and the first thing it found.

## What was ported, and what was not

`falsify` is Python; NeuroForge is C++. Reimplementing the statistics in C++
would have been the wrong port: the value is the experimental discipline, not the
arithmetic.

`tools/nf_experiment.py` runs NeuroForge invocations, parses metrics out of
stdout, and hands seed-keyed dicts to `falsify.paired`. No C++ changed.

Exposed: `run_arm`, `compare`, `within_condition_spread`, `check_metric_moves`,
and a `report()` that prints spread *before* effect and states plainly when an
effect fails to clear it.

## The methodological correction it forces

`falsify.paired` requires arms to share **distinct seeds** and warns below
`MIN_SEEDS = 5`. Every comparison in this session used **one seed (401) repeated
three times**, relying on residual nondeterminism for variation.

Those measure different things:

- repeats at a fixed seed measure the **implementation's noise floor**
- distinct seeds measure whether an effect **survives different initial
  conditions**

The session treated them as interchangeable. Measured directly: within-condition
spread on `mean_distance` is **0.73–0.94 across 8 distinct seeds**, against
**0.08–0.73** from repeats of one seed. Distinct seeds produce substantially more
variation, which is the conservative and correct denominator.

`floor_p` is the other correction. **`floor_p(3) = 0.25`, `floor_p(5) = 0.0625`,
`floor_p(8) = 0.0078`.** At three seeds p<0.05 is not merely unachieved, it is
*arithmetically unreachable*: "not significant" there means "not testable". Six
substrate nulls were reported at n=3. They do not collapse — the effects sat well
inside their spreads and every arm carried a passing manipulation check — but the
statistical framing was never load-bearing, and should not have been stated as
though it were.

## Validating the harness, and getting it wrong first

A harness that cannot reproduce a known result is untrustworthy; one that finds
effects everywhere is worse than useless. So it was checked against both:

- **known positive** — `--learn-policy`, measured by hand at 2.139 → 0.967
- **known null** — `--selection-smoothing=0.1`, ±0.07 with no dose-response

**The first validation reported the known positive as a null**: +0.153, wins 3/8,
p=0.47.

That was a defect in the validation script, not the harness or the result. It
changed **three variables at once** relative to how the effect had been
established: 900 steps instead of 1500, eight distinct seeds instead of one
repeated, and a control that included `--motor-selection --action-credit`. A
control differing from its arm in more than one respect cannot attribute
anything — which is precisely the discipline the harness exists to enforce, and
it was violated in the act of installing it.

Corrected: each arm is now validated in the configuration its effect was
established in. `POLICY_COMMON` for the positive, `SUBSTRATE_COMMON` for the
null.

## The headline result, re-seeded

Original configuration, only the seeding changed — 8 distinct seeds, 1500 steps,
control selecting over input regions:

```
n=8   mean delta -0.908   sd 0.275
wins 8/8   perm p 0.00781 (floor 0.00781)   effect/sem 9.34
CONSISTENT — every seed moved the same direction
effect 0.9083 exceeds within-condition spread 0.7333
```

| | hand-measured (1 seed × 3) | re-seeded (8 seeds) |
|---|---|---|
| control | 2.139 | 1.971 |
| learned policy | 0.967 | 1.062 |
| effect | 1.172 | 0.908 |

**The result holds, and is better supported than before.** `p = 0.00781` *equals
the floor* — the most significant outcome obtainable at n=8 — and every one of
eight independent seeds moved the same direction. The effect is smaller against
the larger, more honest denominator, and still clears it.

## What the harness caught unprompted

Beyond the headline check:

- **Outliers, twice.** "seed 403 is an outlier (delta +1.139 vs +0.0119 for the
  rest); mean without it is +0.0119 — report both", and later seed 402 in the
  re-seeded run (−1.433 vs −0.833). Leave-one-out, not a z-score against the full
  sample, so one large outlier cannot mask itself.
- **Manipulation checks discriminating correctly.** Learned policy PASS (decision
  changes 90.9 → 632.8, 6.96×); smoothing FAIL (90.9 → 98.3, 1.08×). That is the
  difference between "mechanism engaged, no behavioural effect" and "mechanism
  never ran", which several nulls in this project depend on.
- **Power.** "seeds for a 2:1 effect/sem ratio: 43 (have 8)" on the weak arm —
  an explicit statement of what would be needed, rather than a shrug.

## Standing guidance

1. Use `tools/nf_experiment.py` for any comparison intended to support a claim.
2. **At least 6 seeds**, preferably 8. Below 6, p<0.05 cannot be reached.
3. **Distinct seeds, shared across arms.** Repeats at one seed measure the noise
   floor, not robustness.
4. Change **one variable** between control and arm. The first validation here
   failed this and produced an uninterpretable null.
5. Every claim carries a manipulation check. A behavioural null is informative
   only if the mechanism demonstrably did something.

## Files

- `tools/nf_experiment.py` — the adapter
- `tools/validate_harness.py` — positive and null checks, corrected controls
- `tools/recheck_policy.py` — the re-seeded headline result

`FALSIFY_SRC` overrides the falsify location; it defaults to the sibling
`Architecture_Substrate_Research/falsify/src`.
