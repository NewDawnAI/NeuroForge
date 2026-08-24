# Windowed averaging, and the conclusion of the substrate-learning arc

Date: 2026-08-24
Follows: `critic_and_node_perturbation_2026-08-24.md`
Closes: six attempts to make the substrate learn the phototaxis task

## Result

Three smoothing levels, three runs each, 1,500 steps, policy learner off:

| | run means | overall | within-spread |
|---|---|---|---|
| no smoothing | 1.80, 2.08, 1.95 | 1.944 | 0.28 |
| α = 0.10 | 2.13, 2.08, 1.85 | 2.022 | 0.28 |
| α = 0.02 | 1.90, 1.75, 2.02 | 1.889 | 0.27 |

Max within-condition spread **0.283**. Against no-smoothing: α=0.10 gives
**−0.078** (worse), α=0.02 gives **+0.056** (better). Both inside the noise, and
**pointing in opposite directions**.

**The dose-response check is what makes this conclusive.** If selection-point
noise were the binding constraint, averaging harder should have helped
monotonically — α=0.02 averages five times harder than α=0.10 and should have
shown a correspondingly larger effect. Instead the two levels disagree in sign.
That is the signature of noise, not of a weak effect.

The mechanism did what it was supposed to: channel spread fell from ~0.65 to
0.424 at α=0.10, and decision changes fell from ~400 to 40 per run. Selection
genuinely became more stable. Behaviour did not change.

## The six attempts

| # | change | effect | what it established |
|---|---|---|---|
| 1 | three-factor eligibility + action gate | +0.244 | eligibility was flat and ungated |
| 2 | eligibility decay | inside noise | traces never decayed; no timing information |
| 3 | selection on motor channels | +0.011 | credited synapses were off the decision path |
| 4 | scalar reward baseline | +0.006 | diagnosis wrong — score function is not a baseline |
| 5 | critic (TD error) + node perturbation | +0.033 | mechanisms engage; signal below the noise floor |
| 6 | selection smoothing | ±0.07, no dose-response | it is not selection-point noise either |

Attempts 1, 2 and 3 each removed a genuine defect and were necessary. Attempt 4
rested on a misreading. Attempts 5 and 6 built the two remaining candidate
explanations correctly and falsified both.

## What is now established

The substrate has, all verified to engage:

- decaying eligibility traces (λ=0.85/step)
- signed traces, so negative deviations are not discarded
- three-factor `pre × post`, or node-perturbation `pre × (post − E[post])`
- action-specific gating to the selected motor channel
- selection reading the credited pathway
- TD-error modulation from a learned state-dependent critic
- optional temporal averaging of the decision variable

And it does not learn. Every mechanism proposed across six attempts is present,
and each was confirmed to change the system's internal statistics — the clearest
being potentiated/depressed ratios of 1.303/1.315/1.348 against
1.242/1.251/1.239, non-overlapping across independent runs.

**The nulls are well-powered, not uninformative.** That distinction was
maintained deliberately: every arm carried a manipulation check, and every
comparison measured within-condition variance first.

## What this means

The policy layer solves the same task, in the same world, from the same
observations, with the same reward — 0.967 mean distance, agent sitting on the
light. The difference is structural:

| | where the learned parameters sit |
|---|---|
| policy layer | **at** the decision: `score[a] = w[a]·x`, so a weight change *is* a change in the decision |
| substrate | upstream of it, behind region dynamics that must be traversed before selection |

Reward-modulated Hebbian plasticity distributed across a recurrent substrate did
not, here, produce directed behaviour change — while the same reward applied to
parameters sitting directly at the decision did, immediately and reproducibly.

This is a negative result about *this substrate on this task*, not about local
learning rules in general. Stated with the limits it actually has:

- one task, and a simple one — 8 states, 4 actions, near-fully observed
- 1,500 steps per run, three runs per arm
- one architecture, with a specific and shallow credit path
- a linear critic on four hand-chosen features

## Where it could still go

1. **Accept the hybrid.** Policy layer as actor, substrate as representation.
   This is roughly how basal-ganglia models are usually built, and it works
   today. The honest framing of what exists.
2. **Shorten the path from weights to decision.** The measured problem is
   distance, not mechanism. A substrate where the selected action *is* the
   argmax over a directly-credited population — no intervening dynamics — is a
   different architecture, not another flag.
3. **A task with denser reward.** Phototaxis gives one scalar per cycle for a
   4-way choice. Anything richer would raise the signal-to-noise of every
   mechanism already built.

Option 1 is what the codebase is. Option 2 is the real experiment, and it is a
redesign rather than an increment.

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Determinism intact: Thalamus 0.6955 × 3.
- Policy-learner result unaffected: agent at `dist=0 brightness=1.000`.
- Full build: 106/106, 0 errors.
- Test sweep: 33 pass, 3 fail — the pre-existing baseline.

All flags from all six attempts are opt-in and retained. None changes the default
path, and each is documented in `docs/FLAGS_ADDED_2026-08.md` with the
measurement that justifies it and the caveat that limits it.
