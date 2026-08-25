# A second task, a degenerate first one, and three retracted mechanisms

Date: 2026-08-26

Item (1) of the 1→2→3 plan: add a second task so transfer becomes measurable.
The task work is done. The measurements it enabled were more consequential than
the feature, and two of my three interpretations of them were wrong.

## What was built

| flag | effect |
|---|---|
| `--task=photo\|audio\|avoid` | approach a light via VisualCortex / the same gradient via AuditoryCortex / retreat from it |
| `--task-2=` + `--task-switch-at=N` | swap task mid-run, so the second half is a transfer measurement rather than an independent run |
| `--task-randomize=N` | move the light and the agent every N cycles |

`setSensoryRegion()` was added so the policy's spatial features follow the active
cortex. They were hardcoded to `VisualCortex`, which would have made the audio
arm fail for a wiring reason rather than a scientific one — the same state
aliasing that made the original phototaxis policy bearing-blind.

`avoid` is a clean manipulation check: **2.33 against 1.33**, the agent reverses
when the reward is inverted.

## The first task was degenerate

Ablating `VisualCortex` on the fixed world left performance unchanged
(1.33 → 1.33) while ablating every region moved it (1.33 → 1.71). Measured
properly at 6 seeds, the learned policy's advantage was **−0.644 intact and
−0.625 with vision ablated** — vision worth ~3%.

With a fixed source at 4 and a fixed start at 0, a learned action bias reaches
and holds near the light without perceiving anything. A task solvable without its
sensory channel cannot distinguish learning-to-perceive from learning-a-bias, and
makes cross-modal transfer meaningless: two tasks differing in a variable neither
depends on.

`--task-randomize` fixes this. Vision's measurable contribution rose from **~3%
to ~19.5%**:

| | intact | vision ablated | vision's share |
|---|---|---|---|
| fixed world | −0.644 | −0.625 | ~3% |
| randomised | −0.911 | −0.733 | ~19.5% |

Both 6/6 consistent, both exceeding their spreads. **The randomised task should
be the default for any future claim about perception.**

## The learning is real, and it is not movement

The control I should have had from the start. Comparing a learner against a
*static* argmax confounds learning with being dynamic at all. `--policy-temp=100`
makes the softmax near-uniform: the policy still moves, but its weights barely
influence which way.

| comparison | effect | wins | effect/sem |
|---|---|---|---|
| near-random vs control | −0.125 | 3/6 | 1.34 — **null** |
| learned vs control | −0.867 | 6/6 | 10.81 |
| **learned vs near-random** | **−0.742** | **6/6** | **30.59** |

Movement contributes nothing. The learning, isolated from it, is the strongest
and tightest result measured in this project.

## Three mechanisms proposed, two retracted

| # | claim | verdict | what killed it |
|---|---|---|---|
| 1 | the advantage is movement, not learning | **wrong** | near-random arm is null (−0.125, 3/6) |
| 2 | the policy is not using vision | **wrong** | it uses it for ~19.5% on the randomised task; the fixed-task result reflected a degenerate task, not blindness |
| 3 | Thalamus relays the visual signal, which is why ablating VisualCortex cost little | **wrong** | ablating Thalamus costs 0.014 against VisualCortex's 0.178 |

**The recurring error: reading an ablation as evidence about what a system USES.**
An ablation shows what is *necessary*. A signal available through more than one
route, or a task solvable more than one way, will survive removal of any single
path — and that survival says nothing about whether the path is used. I made this
inference three times in three different forms before naming it.

The correct instrument for "what does the policy key on" is not ablation but
**direct inspection of the learned weights**. `PrefrontalCortex::policyWeights()`
is already exposed and was not used.

## What is established, and what is not

**Established:**
- learning is real, large, and not movement (−0.742, effect/sem 30.59)
- the fixed task was solvable without perception; the randomised one is much less so
- vision contributes ~19.5% of the advantage on the randomised task
- Thalamus contributes ~2%
- `avoid` inverts behaviour, confirming the reward path

**Not established:**
- what carries the remaining ~80%. No mechanism I tested accounts for it, and I
  am not proposing a fourth without an instrument that can distinguish it.

**Not yet measured at all:** transfer. That was item (1)'s purpose. The task
machinery exists, but running a transfer experiment before the single-task
mechanism is understood would produce a number without an interpretation — which
is precisely the failure this note documents three instances of.

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Full build: 106/106, 0 errors.
- All new flags opt-in; the default world is still the fixed one, so prior
  measurements remain reproducible.
