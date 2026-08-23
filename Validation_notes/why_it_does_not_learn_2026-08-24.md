# Phase-4 runs now. It still does not learn, and here is why.

Date: 2026-08-24
Follows: `closed_loop_2026-08-24.md`

Two things were asked: fix the eligibility representation, and explain why the
brain is not learning. The first is done and turned out not to be the real
problem. The second has a definite answer.

## Headline

**Phase-4 Updates: 237,409**, from 0 for the entire session. Reward-modulated
plasticity is running for the first time.

**The behaviour is unchanged.** Choice distribution with learning off and on:

```
off: {0: 18%, 1: 46%, 2: 33%, 3: 2%}   n=517
on : {0: 18%, 1: 46%, 2: 34%, 3: 2%}   n=510
```

237,409 weight updates moved the policy by one percentage point in one bucket.

## 1. The eligibility "performance problem" was a deadlock

`--auto-eligibility=on` appeared to make the anatomical brain 100x slower, and
survived four separate optimisations aimed at that theory:

- an O(1) `Region::getNeuron` index replacing a `find_if`,
- `Neuron::collectSynapseIds()` removing two `shared_ptr` vector copies per spike,
- a cache for `findSynapseById`, which had scanned the whole connectome per lookup,
- `EligibilityTraces`, a flat lock-free array replacing
  `unordered_map<SynapseID, SynState>` under a global mutex.

None of them helped, because none addressed the actual fault. Two measurements
found it. First, `--steps=1` timed out exactly like `--steps=20` — cost that does
not scale with work is not a performance problem. Second, a probe inside
`onNeuronSpike` printed **9 "about to getNeuron" and 8 "getNeuron returned"**.

`getNeuron` took `region_mutex_`. Neuron spike callbacks fire from inside code
that already holds it, and `std::mutex` is not recursive, so the lookup deadlocked
against its own caller on the first spike. The fix is one line of intent:
`neuron_index_` now has its own `neuron_index_mutex_`, and `getNeuron` never
touches `region_mutex_`.

After: the same run completes in **1 s**, and the full closed loop with learning
in **21 s**.

The four optimisations are kept — each is a real defect and each was measured —
but the honest account is that this was a hang misdiagnosed as slowness, and four
rounds of optimising were spent before measuring the thing itself. `--steps=1`
timing out should have been checked first.

## 2. It is still not learning

Six runs of the phototaxis task, three per condition, 600 steps each:

| | run means | overall |
|---|---|---|
| learning off | 2.04, 1.71, 1.83 | **1.861** |
| learning on | 2.00, 1.67, 1.54 | **1.736** |

Random walk on an 8-ring is ~2.0, standing on the light is 0.

The difference between conditions (0.125) is smaller than the spread **within**
either condition (0.33 off, 0.46 on). Early-versus-late within runs shows no
consistent improvement — learning-off went 1.92→2.17, 1.42→2.00, 1.75→1.92, and
learning-on went 1.75→2.25, 1.83→1.50, 1.67→1.42. Nothing here is
distinguishable from chance at n=3.

The choice distribution above settles it independently: the policy is the same.

## 3. Why — four reasons, in order of how fundamental they are

### 3.1 The policy has no learnable parameters

`PrefrontalCortex::makeDecision` is, in full:

```cpp
// Simple decision making: select option with highest value
auto max_it = std::max_element(values.begin(), values.end());
decision.selected_option = std::distance(values.begin(), max_it);
decision.confidence = *max_it;
```

A hardcoded argmax. There is no weight matrix, no policy state, nothing that
reward can adjust. Weights reach the decision only indirectly — weights change
activations, activations become `values`, argmax picks one — and that path
carries no memory of which action was rewarded.

### 3.2 Eligibility carries no credit assignment

`onNeuronSpike` bumps **every** synapse of **every** spiking neuron by a flat
0.1. With 83% of neurons active in the anatomical brain, eligibility is close to
uniform across the network. It records "was recently active", not "contributed to
the action that was taken".

Credit assignment is the entire content of a reinforcement learning algorithm. A
uniform trace is not a weak version of it; it is its absence.

### 3.3 Therefore the weight update is a global nudge

`dw = kappa * R * eligibility * lr` with near-uniform eligibility is
approximately a single scalar applied to every recently-active synapse. That
shifts all activations in the same direction and largely preserves their
ordering — and an argmax only cares about ordering. Hence 237,409 updates and an
unchanged policy.

### 3.4 The action mapping is arbitrary and fixed

`choice % 4` maps to left / right / hold / hold. Nothing associates "moving left
from here was rewarded" with any parameter anywhere. Even a perfect credit signal
would have nothing to attach itself to.

## 4. What this means

The loop is genuinely closed: sense → decide → act → consequence → reward →
plasticity, all executing, all measured. What is missing is not a wire but an
**algorithm**. There is no policy gradient, no value function, no action-specific
eligibility, no exploration schedule — the machinery that would turn a reward
signal into a behavioural change does not exist in this codebase yet.

That is a design gap, not a bug, and it is worth stating plainly because
everything upstream now works and could easily be mistaken for a system that
should be learning.

The minimum that would make this task learnable:

1. **Action-specific eligibility.** Tag the synapses that drove the chosen action
   rather than every synapse that fired. Without this nothing else helps.
2. **A parameterised policy.** Replace the argmax with something whose parameters
   reward can move — even a linear preference per action would do.
3. **Exploration.** An argmax over near-tied values is nearly deterministic; a
   policy that never tries an alternative cannot discover a better one.

Item 2 is the smallest change with the clearest test: the phototaxis baseline of
**1.861 mean distance (learning off)** is already recorded here, along with the
within-condition spread needed to judge whether any future change beats it.

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Anatomical determinism intact: Thalamus 0.6955 x 3.
- n=8192 still runs: 1,048,576 synapses.
- Full build: 106/106, 0 errors.
- Test sweep: 33 pass, 3 fail — the pre-existing baseline.
- Debug probes added during diagnosis were removed; the file is asserted clean.
