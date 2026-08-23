# Connecting a sensory stream: wired, propagating, not yet decisive

Date: 2026-08-23
Follows: `unified_brain_integration_2026-08-23.md`

The prefrontal decision was made state-dependent, but on the brain's *own*
activity — a closed loop deciding about itself. This connects an external input
and measures how far it travels.

## Result in one line

The sensory pathway is **causal and reproducible** all the way to prefrontal
cortex, and it does **not** change which option the brain selects.

## What was added

`--sensory-drive` injects the synthetic moving-checker grid — the same pattern
the vision demo falls back to without a camera — into the anatomical
`VisualCortex` each step via `processVisualInput()`. `G=8` gives 64 values,
matching `--anatomical-neurons=64` one-to-one. It is deterministic and it changes
every step.

Like the integration binding, it reports when it cannot act rather than failing
silently:

```
[Sensory] no VisualCortex region -- drive INERT (enable --anatomical-regions)
[Sensory] VisualCortex present but is a base Region -- drive INERT (...)
```

Also added a per-region activation breakdown to the run summary, because the
aggregate counts hide *where* signal lives, which is exactly what this question
needs.

## The pathway is real

Three runs per condition, `--steps=60 --sequential`. **Zero variance within each
condition** — every run gave identical values, so these differences are the
effect and not noise:

| region | drive off | drive on | delta |
|---|---|---|---|
| VisualCortex | 0.4900 | 0.5041 | +0.0141 |
| Thalamus | 0.6955 | **0.7367** | **+0.0412** |
| PrefrontalCortex | 0.1780 | **0.1562** | **-0.0218** |

VisualCortex's active-neuron count also drops from 64 to **32** under drive —
exactly half, which is what an 8x8 checkerboard should produce. That is a
sanity check on the injection landing correctly, not just changing a number.

So the chain holds: **input → VisualCortex → Thalamus → PrefrontalCortex**, which
is the anatomical wiring this brain was built with.

## The decision does not move

At `--steps=250` with the autonomous loop running, the choice sequence is
**identical** in both conditions:

```
drive OFF: 0 1 3 0 1 0
drive ON : 0 1 3 0 1 0
```

The decision *inputs* differ, but only in the sixth decimal at the sampled
moments:

```
off: PFC option 0 of 4 conf=0.984375 inputs=[0.563257 0.215687 0.187091 0.454384]
on : PFC option 0 of 4 conf=0.984375 inputs=[0.563256 0.215687 0.187091 0.454384]
```

### Why, most likely

The four options carry very different baselines — Thalamus around 0.56-0.70
against Hippocampus around 0.19-0.22 and Amygdala around 0.19. A sensory
perturbation of ~0.04 on the thalamic channel cannot reorder a set that is
separated by ~0.35. The selection is dominated by **which region this is** rather
than **what the region is currently seeing**.

Stated as a hypothesis, not a conclusion: it follows from the measured numbers
but has not been tested by manipulating the baselines directly.

## What this does and does not establish

**Does:** the sensory input arrives, propagates through two synaptic stages, and
measurably changes prefrontal activity, reproducibly and with zero run-to-run
variance. The wiring is not decorative.

**Does not:** that the brain behaves differently because of what it sees. On this
evidence it does not. Reporting the activation deltas as though they were
behavioural change would be the same error as reading "integration ENABLED" as
"integration working" — the thing this session has now hit five times.

## Where it would go next

In rough order of what the measurements point to:

1. **Normalise the decision inputs.** If each channel were expressed relative to
   its own recent baseline rather than as a raw mean, a 0.04 thalamic shift
   would be a large relative change instead of a small absolute one. This is the
   direct implication of the diagnosis above and is testable: normalising should
   make the choice sequence move under drive.
2. **Drive more than one modality.** Only VisualCortex is fed;
   SomatosensoryCortex sits at 0.8613 and AuditoryCortex at 0.4516 on internal
   dynamics alone, so they contribute constant background to the thalamic
   summary.
3. **Give the decision consequences.** The motor command is planned and executed
   but nothing observes the result, so there is no signal that one choice was
   better than another. Until then the decision cannot be *learned*, only made.

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Region activations reproducible: 3/3 identical in both conditions.
- Full build: 106/106, 0 errors.
- Test sweep: 33 pass, 3 fail — the pre-existing baseline.
