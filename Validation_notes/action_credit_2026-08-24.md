# Action-specific credit: two real fixes, and why the substrate still does not learn

Date: 2026-08-24
Follows: `it_learns_2026-08-24.md`

Goal: make the **substrate** learn, rather than a policy layer beside it. Two
genuine defects in the eligibility machinery were fixed. The substrate still does
not learn, and the reason is now provable rather than suspected.

## Result

1,500 steps, three runs per condition, policy learner **off** so only the
substrate can be responsible:

| | run means | overall | within-spread |
|---|---|---|---|
| uniform credit | 2.18, 2.00, 2.47 | 2.217 | 0.47 |
| gated credit | 2.23, 1.88, 1.80 | **1.972** | 0.43 |

Effect **+0.244** against a max within-condition spread of **0.467**. The
difference is inside the noise, and there is no learning curve — quarters run
`2.20 → 3.20 → 1.53 → 2.00`, against the policy learner's consistent
`1.67 → 0.60 → 0.80 → 0.60`.

**Not a result.** Reported as a null.

## 1. Fixed: eligibility was flat and ungated

`onNeuronSpike` bumped every synapse of every spiking neuron by a constant 0.1.
With 83% of neurons active, that records "was recently active" rather than "was
responsible".

Now a **three-factor** increment, `rate × pre_activation × post_activation`,
computed per synapse from its own endpoints. A synapse whose target just spiked
but whose source was silent contributed nothing and now earns nothing.

`--action-credit` additionally confines accumulation to the motor channel that
carried the selected action. Verified live:

```
[Credit] eligibility gated to 16 of 64 MotorCortex neurons (channel 0 of 4)
```

That is the basal-ganglia arrangement: the selected channel is eligible, the
losing channels are not.

## 2. Fixed, and more important: the traces never decayed

`bump()` saturates at a cap, and nothing on the spike path ever reduced a value.
A comment at the Phase-4 site read *"eligibility decay removed to match expected
test formula kappa*R*elig"*. `clear()` ran only at init and on reconfigure.

So every synapse that fired once climbed to the cap and stayed there for the rest
of the run. **That is not a trace — it is a "has ever been active" flag**, and it
carries no timing information whatsoever. Reward could not be associated with a
recent action even in principle.

It also explains a measurement that made no sense at the time: gating produced
**more** Phase-4 updates than not gating (451,330 against 431,198). The gate
arrived after everything had already saturated, so it could not reduce anything.

Added `EligibilityTraces::decay(factor)`, applied once per learning step after
the reward path consumes traces, default λ=0.85.

Both fixes are correct and both were needed. Neither was sufficient.

## 3. Why it still does not learn — the credited synapses cannot affect the choice

The prefrontal decision reads its options from:

```cpp
static const char *kSources[] = {
    "Thalamus", "Hippocampus", "Amygdala", "CingulateCortex"};
```

plus four spatial features from `VisualCortex`. The action gate puts credit on
**MotorCortex** input synapses. Grepping the decision inputs for `MotorCortex`
returns **0**.

So strengthening the credited synapses cannot change which action
`argmax(values)` selects. Reward reaches a pathway that has no influence on the
choice. The loop is open at exactly that junction, and no amount of better credit
assignment on the motor side can close it.

This is the same class of error as the state aliasing found in
`it_learns_2026-08-24`: the machinery was correct and pointed at the wrong place.

## 4. What would actually close it

The decision must be an argmax (or softmax) over **action values held in the
motor channels themselves**, not over sensory and limbic region activations. That
is the standard actor-critic and basal-ganglia arrangement: action values live in
the striatal/motor channels, selection reads those channels, and reinforcement of
a channel therefore changes the next selection.

Concretely:

1. Partition `MotorCortex` into one channel per action (already done for the
   gate).
2. Have the decision read **channel activations** as its options, so the choice
   is over actions rather than over input regions.
3. Keep the sensory and limbic regions as *inputs to* those channels — which is
   what the anatomical wiring already provides, since prefrontal drives motor.
4. Keep the gated three-factor eligibility as it now stands; it becomes
   load-bearing the moment step 2 exists.

Steps 1 and 4 are done. Step 2 is the architectural change, and it is not small:
it moves action selection from "which input region is most active" to "which
action channel has the highest value", which is a different model of what the
prefrontal cortex is doing.

## 5. State of play

| | status |
|---|---|
| Policy learner solves phototaxis | yes — 2.139 → 0.967, agent sits on the light |
| Substrate learns | **no** — credited synapses do not reach the decision |
| Eligibility is a real trace | yes, now — decays at λ=0.85/step |
| Eligibility is action-specific | yes, now — 16 of 64 motor neurons per action |
| Credit reaches the decision path | **no** — this is the remaining gap |

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Determinism intact: Thalamus 0.6955 × 3.
- Policy-learner result still holds: agent at `dist=0 brightness=1.000` at
  iteration 1475.
- Full build: 106/106, 0 errors.
- Test sweep: 33 pass, 3 fail — the pre-existing baseline.

## Method note

Two false negatives during this work came from the same shell trap: a
`grep -c` returning zero matches exits non-zero, which short-circuited an `&&`
chain so the binary never ran, and the empty output was read as "the gate is
never set". The gate had been working throughout. Third occurrence this session;
capture to a file and grep the file rather than chaining.
