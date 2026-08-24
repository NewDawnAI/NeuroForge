# Selection on action channels: the junction is closed, the substrate still does not learn

Date: 2026-08-24
Follows: `action_credit_2026-08-24.md`

The previous note ended with a provable diagnosis: reward was gated onto
MotorCortex synapses while selection read Thalamus/Hippocampus/Amygdala/
CingulateCortex, so credited synapses could not influence the choice. This
implements the fix — selection now reads the motor channels themselves — and
reports the result.

## Result

1,500 steps, three runs per condition, policy learner **off**:

| | run means | overall | within-spread |
|---|---|---|---|
| uniform credit | 2.00, 2.10, 1.78 | 1.961 | 0.32 |
| gated credit | 1.87, 1.85, 2.13 | 1.950 | 0.28 |

Effect **+0.011** against a within-condition spread of **0.317**. Essentially
zero — and *smaller* than the +0.244 measured before this change. Both arms sit
at the random-walk baseline (~2.0). No learning curve in any run.

**A null.** The architectural fix was necessary and is not sufficient.

## 1. The change, and evidence it works

`readRegionChannel(region, channel, n_channels)` reads a contiguous slice of a
region's neurons and returns its mean activation and its above-threshold
fraction. With `--motor-selection`, the prefrontal decision's options become the
four MotorCortex action channels rather than the input regions; the sensory and
limbic regions remain *inputs to* those channels, which is what the anatomical
wiring already provides.

This is the actor-critic and basal-ganglia arrangement: action values live in the
motor channels, selection reads those channels, and reinforcing a channel
therefore changes the next selection.

Verified live:

```
[Policy] selection reads MotorCortex action channels
[Decision] PFC option 3 of 4 conf=0.581 norm=[0.579 0.808 0.838 0.33]
[Credit]  eligibility gated to 16 of 64 MotorCortex neurons (channel 0 of 4)
```

Selection is over 4 actions, the channels carry visibly different values, all
four actions are chosen, and the credit gate is active. The junction that was
open in `action_credit_2026-08-24` is closed.

## 2. Why it still does not learn — no baseline

Reward reaches the weights. It is not a plumbing failure:

| | Phase-4 updates | avg weight change | potentiated | depressed |
|---|---|---|---|---|
| uniform | ~2.0–2.7M | 3.0–3.7e-4 | 5.5–5.9M | 4.1–4.5M |
| gated | ~1.9–2.2M | 3.8–4.9e-4 | 5.5–5.8M | 4.1–4.2M |

Potentiation and depression are **nearly balanced**, roughly 1.3:1, in every run
and both conditions. Weights move constantly and go nowhere.

The rule is

```
dw = kappa * R * eligibility * global_learning_rate
```

with **no baseline**. Reward here is the *change* in brightness, which for a
near-random policy has a mean of approximately zero: an action that closes the
distance is rewarded and the next that opens it is punished by a similar
magnitude. The update therefore alternates sign and cancels. It is a
high-variance, zero-mean random walk on the weights, which is exactly what the
potentiated/depressed ratio shows.

The learned policy succeeds on the **same reward signal** because its update is

```
dw[a][f] += lr * R * (1[a == chosen] - p[a]) * x[f]
```

The `(1[a==chosen] - p[a])` term is an advantage — a baseline subtracted from the
action's probability. That is what turns a zero-mean reward into a directed
update. The substrate's rule has no equivalent term.

**This is the missing piece, and it is a different problem from credit
assignment.** Credit assignment says *which* synapses; a baseline says *how much
better than expected* the outcome was. Fixing the first without the second gives
correctly-targeted noise.

## 3. Three attempts, and what each established

| attempt | change | result | what it established |
|---|---|---|---|
| 1 | three-factor + action gate | +0.244, inside noise | eligibility was flat and ungated; traces never decayed |
| 2 | (same, after decay fix) | still inside noise | credited synapses were not on the decision path |
| 3 | selection on motor channels | +0.011 | the path is now closed; the rule has no baseline |

Each attempt fixed something real and each was necessary. None was sufficient,
and the sequence is worth keeping: three defects had to be removed before the
fourth became visible.

## 4. What would close it

A **reward baseline**, i.e. a critic. Concretely, maintain a running estimate of
expected reward and apply

```
dw = kappa * (R - R_baseline) * eligibility * lr
```

An exponential moving average of recent reward is the minimum viable form and is
biologically defensible: dopamine encodes reward *prediction error*, not reward.
That is precisely the term missing here, and the substrate is currently
implementing the pre-Schultz model — raw reward rather than prediction error.

Estimated effort: small. The measurement is already set up, the baseline for
comparison is recorded (uniform 1.961, gated 1.950, spread 0.317), and the
policy-learner result (0.967) gives a target.

## 5. State of play

| | status |
|---|---|
| Eligibility is a decaying trace | yes |
| Eligibility is three-factor (pre × post) | yes |
| Eligibility is action-specific | yes |
| Credited synapses influence the choice | **yes, now** |
| Reward reaches the weights | yes — ~2M updates/run |
| Weights converge | **no** — potentiation and depression cancel |
| Substrate learns | **no** — no reward baseline |
| Policy layer learns | yes — 0.967, agent sits on the light |

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Determinism intact: Thalamus 0.6955 × 3.
- Policy-learner result unaffected: agent at `dist=0 brightness=1.000` at
  iteration 1475.
- Full build: 106/106, 0 errors.
- Test sweep: 33 pass, 3 fail — the pre-existing baseline.

`--motor-selection` is opt-in; the default decision path is unchanged.
