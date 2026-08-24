# Critic and node perturbation: the mechanisms work, the substrate does not learn

Date: 2026-08-24
Follows: `reward_baseline_2026-08-24.md`

Both remaining directions from the previous note are now implemented: a
state-dependent critic supplying TD error, and node perturbation making the
eligibility trace a local gradient estimator rather than a coincidence measure.

Both engage, verifiably. Neither produces learning. This note localises why.

## Result

1,500 steps, three runs per condition, policy learner off, with credit
assignment and motor-channel selection already correct:

| | run means | overall | within-spread |
|---|---|---|---|
| control | 1.60, 2.02, 2.08 | 1.900 | 0.48 |
| `--critic --node-perturbation` | 1.78, 1.78, 2.03 | 1.867 | 0.25 |

Effect **+0.033** against a within-condition spread of **0.483**. Both arms at
the random-walk baseline. Fifth null in a row on the substrate.

## 1. The manipulation check passes

This is what separates this null from an uninformative one. The potentiated /
depressed ratio, per run:

| control | critic+NP |
|---|---|
| 1.303 | 1.242 |
| 1.315 | 1.251 |
| 1.348 | 1.239 |

**No overlap.** The mechanisms demonstrably change the weight-update statistics,
reliably, across independent runs. They are not inert — they are ineffective.

## 2. What was built

**Critic** (`--critic`, `--critic-lr=`, `--critic-gamma=`). Linear V(s) learned
online by semi-gradient TD; the Phase-4 modulator becomes
`R + gamma*V(s') - V(s)`.

Two deliberate choices:

- **Features are what the brain perceives**, not what the world reports: the
  left/right halves of the visual field and their difference, read off
  VisualCortex neurons. Feeding it the world's `dist` would make the critic an
  oracle and the result would not transfer to a task without one.
- **`observeState` fires after the new observation is injected**, so the value
  formed is genuinely V(s') for the reward delivered that cycle, which Phase-4
  consumes on the next step. Weights start at zero, so early TD errors are the
  rewards themselves.

**Node perturbation** (`--node-perturbation`). The trace becomes
`rate * pre * (post - E[post])`, with E[post] a per-neuron running expectation.
The deviation is the perturbation; correlating it with reward estimates which way
the weight should move — the same role `(1[a==chosen] - p[a])` plays for an
explicit softmax, but computed from quantities available at the synapse, which
preserves locality.

**A prerequisite that would have silently broken it:** traces had to become
**signed**. `bump()` clamped to `[.., cap]` and `decay()` tested `next < floor`,
so every negative trace would have been zeroed on its first decay. Node
perturbation produces a negative trace whenever a neuron fires below its own
expectation, and that sign is the information — it says the synapse pushed
activity the wrong way. Both now compare magnitude. Without this the null would
have been measured with half the signal discarded and no indication of it.

## 3. Where the chain actually breaks

Traced end to end:

| link | status |
|---|---|
| reward reaches the weights | **yes** — ~2.5M Phase-4 updates per run |
| the mechanisms change the updates | **yes** — ratio 1.30 vs 1.24, no overlap |
| motor channels differentiate | **yes** — mean spread 0.65, e.g. `[0.50, 0.02, 0.91, 0.91]` |
| differentiation grows with learning | **no** — 0.627 early, 0.685 late |
| selection becomes state-dependent | **no** — choices stay ~uniform, 25% each, early and late |

The channels vary strongly from moment to moment, and selection follows that
variation faithfully. What it does not do is come to reflect *which action is
good in this state*. The intrinsic fluctuation in channel activation (spread
~0.65) swamps the learned component: a weight change of order 1e-4 per update
never rises above the noise at the point where the argmax is taken.

**This is a signal-to-noise problem at the selection point, not a missing
mechanism.** Every mechanism proposed across five attempts is now present and
verified to engage.

## 4. Five attempts

| # | change | result | established |
|---|---|---|---|
| 1 | three-factor eligibility + action gate | +0.244 | eligibility was flat and ungated |
| 2 | eligibility decay | inside noise | traces never decayed; no timing information |
| 3 | selection on motor channels | +0.011 | credited synapses were off the decision path |
| 4 | scalar reward baseline | +0.006 | diagnosis wrong — score function is not a baseline |
| 5 | critic (TD error) + node perturbation | +0.033 | mechanisms engage; learned signal is below the noise floor |

Attempts 1–3 each removed a real defect. Attempt 4 was based on a misreading.
Attempt 5 built both remaining candidate mechanisms correctly and found the
limit is elsewhere.

## 5. What this says about the architecture

The policy layer solves this task (0.967, agent sits on the light) using the same
reward signal, the same world, and the same observations. The difference is that
its parameters sit **directly** at the selection point — `score[a] = w[a]·x`, so
a weight change *is* a change in the decision.

In the substrate, a weight change has to propagate through region dynamics before
it reaches selection, and those dynamics have a fluctuation amplitude far larger
than the per-update change. The gap is not the learning rule any more. It is that
**the learned parameters are too far from the decision, relative to the noise
between them.**

Two honest options:

1. **Reduce the noise between weights and selection** — e.g. average channel
   activation over a window before selecting, so transient fluctuation cancels
   and the learned component accumulates. Cheap to try, and directly targets the
   measured cause.
2. **Accept the hybrid.** The policy layer is a legitimate actor; the substrate
   provides representation. That is close to how basal-ganglia models are usually
   built, and it already works.

Option 1 is the honest next experiment. Option 2 is what the codebase currently
is.

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Determinism intact: Thalamus 0.6955 x 3.
- Policy-learner result unaffected: agent at `dist=0 brightness=1.000`.
- Full build: 106/106, 0 errors.
- Test sweep: 33 pass, 3 fail — the pre-existing baseline.

All flags are opt-in; the default learning path is unchanged.
