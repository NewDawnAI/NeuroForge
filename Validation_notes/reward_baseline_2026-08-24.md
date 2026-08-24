# The reward baseline: a null, and a correction to the diagnosis that motivated it

Date: 2026-08-24
Follows: `motor_selection_2026-08-24.md`

That note ended by prescribing a reward baseline, on the grounds that the learned
policy's `(1[a==chosen] - p[a])` term was "an advantage term, i.e. a baseline".
**That reading was wrong.** The baseline was implemented anyway, tested, and is a
null. This note records both.

## Result

1,500 steps, three runs per condition, policy learner off, with credit assignment
and motor-channel selection both already correct:

| | run means | overall | within-spread |
|---|---|---|---|
| no baseline | 1.75, 2.12, 1.78 | 1.883 | 0.37 |
| `--reward-baseline` | 1.78, 1.78, 2.07 | 1.878 | 0.28 |

Effect **+0.006** against a within-condition spread of **0.367**. Both arms at
the random-walk baseline (~2.0). No learning curve.

**A null, and it was predicted before the run** — from the mechanism check below,
not after the fact.

## 1. The correction

`(1[a==chosen] - p[a]) · x` is not a baseline. For a softmax policy it is
**∇log π(a|s)**, the score function. The learned policy's update

```
dw[a][f] += lr * R * (1[a == chosen] - p[a]) * x[f]
```

is `lr · R · ∇log π`, which is REINFORCE — a **policy gradient**, with no
baseline in it at all. Calling it an advantage term conflated two different
things and produced a prescription that could not have worked.

The two rules differ more fundamentally than the previous note claimed:

| | update | what it optimises |
|---|---|---|
| policy layer | `lr · R · ∇log π(a\|s)` | expected reward, by gradient ascent |
| substrate | `kappa · R · eligibility` | nothing in particular — reward-modulated Hebbian |

Eligibility is a **coincidence** trace (pre × post), not the gradient of a
log-probability. Multiplying reward by coincidence strengthens whatever happened
to be co-active when reward was positive. That is a real learning rule, and it is
not gradient ascent on expected reward. No baseline converts one into the other.

## 2. Why the baseline is a no-op here specifically

Independently of the above: an exponential moving average converges to the
**mean** of the reward. In this task reward is the *change* in brightness, so for
a near-random policy its mean is approximately zero. `R - b ≈ R`, and the update
is unchanged.

Measured before running the behavioural test, at 400 steps:

| | potentiated | depressed | ratio |
|---|---|---|---|
| no baseline | 1,849,211 | 1,189,770 | 1.554 |
| baseline | 1,811,475 | 1,177,959 | 1.538 |

The flag was confirmed active (`[Learning] reward baseline ON rate=0.01`) and the
balance barely moved. That is what a no-op looks like, and it is why the
behavioural null was predicted rather than discovered.

**A baseline only reduces variance when the reward has a non-zero mean.** A
baseline that would help here has to be **state-dependent** — V(s), estimating
expected reward *from this state* — which is a critic, and a substantially larger
piece of work than a running scalar.

## 3. What the substrate would actually need

Two viable directions, both larger than anything attempted so far:

1. **A critic.** Learn V(s) alongside the policy and use `R + γV(s') - V(s)` as
   the modulator. This is TD error, and it is what dopamine is usually modelled
   as encoding. It makes the modulator informative even when raw reward is
   mean-zero, because it measures whether the *state improved*.

2. **An eligibility trace that approximates ∇log π.** The substrate's trace is
   Hebbian coincidence. Node-perturbation methods get a gradient estimate by
   correlating injected noise with reward — the trace becomes
   `(actual_activation - expected_activation) × input`, which is a local
   approximation to the score function. That keeps learning local, which is the
   architectural commitment this project has made.

(2) is the more interesting one for this codebase, because it preserves locality.
(1) is more standard and more likely to work first.

## 4. Four attempts, and the shape of the sequence

| # | change | result | what it established |
|---|---|---|---|
| 1 | three-factor eligibility + action gate | +0.244 | eligibility was flat and ungated |
| 2 | eligibility decay | inside noise | traces never decayed, so carried no timing |
| 3 | selection on motor channels | +0.011 | credited synapses were off the decision path |
| 4 | reward baseline | +0.006 | reward-modulated Hebbian is not policy gradient |

Each of 1–3 fixed a genuine defect and each was necessary; 4 fixed nothing
because the diagnosis behind it was wrong. The sequence is worth keeping: three
real defects had to be removed before the fourth question could even be asked
clearly, and asking it clearly is what exposed the misreading.

The substrate now has correct, decaying, action-specific, three-factor
eligibility feeding a closed credit path — and still does not learn, because the
**learning rule itself** is not one that optimises expected reward.

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Determinism intact: Thalamus 0.6955 × 3.
- Policy-learner result unaffected: agent at `dist=0 brightness=1.000`.
- Full build: 106/106, 0 errors.
- Test sweep: 33 pass, 3 fail — the pre-existing baseline.

`--reward-baseline` is kept and is off by default. It is correctly implemented
for the case where reward has a non-zero mean; that case simply is not this task.
