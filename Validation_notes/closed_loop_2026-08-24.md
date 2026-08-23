# Closing the loop: the loop closes, the learning does not

Date: 2026-08-24
Follows: `normalised_decision_inputs_2026-08-23.md`

The brain decided, and the decision reached a motor command, but nothing observed
the result — so a changed decision was never a better one, and Phase-4
reward-modulated plasticity reported 0 updates. This gives the decision a
consequence.

## Result, split

**The loop closes.** sense → decide → act → consequence → reward → deliver, at
3s for 20 steps. The agent moves, reaches the light, and reward is delivered on
every cycle.

**The learning does not run.** Phase-4 needs eligibility traces, and accumulating
them is O(spikes x synapses-per-neuron) under one global mutex. With eligibility
enabled the same 20 steps do not complete in 200 seconds. Three separate
optimisations along the way each helped and none was sufficient.

## 1. The world

`LightWorld` is phototaxis on an 8-position ring: a light at a fixed position, an
agent at another, brightness falling off with distance. Reward is the **change**
in brightness, so closing the distance is rewarded and opening it is punished —
absolute brightness would reward standing still near the light.

The observation carries both level (how close) and a left/right imbalance (which
way). Without the second the task is not solvable; without the first there is
nothing to optimise.

The moving checkerboard behind `--sensory-drive` cannot serve here: its mean
brightness is identical at every shift, so no action can change any objective
computed from it. That is why a new environment was needed rather than reusing
the existing input.

Ordering in the pre-cycle hook is deliberate. The hook runs at the **top** of a
cycle, so it applies the decision made on the **previous** cycle, scores the
outcome, and only then presents the observation the next decision will use. That
one-cycle delay is what eligibility traces are for: the trace laid down when the
action was chosen is still present when the reward arrives.

`HypergraphBrain::getLastDecision()` was added to make this possible at all — the
decision was previously made inside `runAutonomousLoop`, used to plan a movement,
and discarded, so nothing outside the brain could see what it chose.

Behaviour over 400 steps: the agent moves, reaches the light (`dist=0`) several
times, and averages distance **1.81** against ~2.0 for a random walk on an
8-ring. With 16 samples and no learning active that is **not** distinguishable
from chance, which is the correct baseline to have established.

## 2. Two flags that silently did nothing

`--auto-eligibility` (bare) is **not accepted** by the live parser, which only
matches `--auto-eligibility=<value>`. Passing the bare form produced

```
Warning: unrecognized option '--auto-eligibility' (ignored)
```

and Phase-4 stayed at 0. The warning exists, but it scrolls past in a run that
prints hundreds of lines, and the failure it predicts is silent.

Underneath that, `auto_eligibility_accumulation_enabled_` defaults to **false**,
so `dw = kappa * R * eligibility * lr` is always `kappa * R * 0`. Reward can be
delivered correctly and change nothing. This is the same enabled-but-inert shape
as `--enable-learning` with zero rates, `--homeostasis` with eta=0, and
`enable_structural_plasticity` with zero batches — now the fifth instance.

## 3. Three real performance defects, fixed

Each was found by measurement and each is a genuine improvement, kept regardless
of the outcome above.

**`Region::getNeuron` was O(n).** A `std::find_if` over `neurons_` under
`region_mutex_`. `onNeuronSpike` calls it once per region per spike, making
eligibility O(spikes x regions x neurons); `connectToRegion` also calls it twice
per synapse. Now an `unordered_map` index maintained at the three sites that
mutate `neurons_`.

**`onNeuronSpike` copied two shared_ptr vectors per spike.**
`getInputSynapses()` and `getOutputSynapses()` each return a copy, so every spike
paid two allocations and a refcount per synapse — for data where only the ids
were used. Added `Neuron::collectSynapseIds()`, writing plain ids into a
thread_local buffer reused across spikes.

**`findSynapseById` scanned the whole connectome per lookup.** Every region's
internal synapses, every input connection vector, every output connection vector,
searching for one id. The Phase-4 reward path calls it once per synapse carrying
eligibility, so with reward every cycle that became thousands of full scans per
cycle. Now cached `id -> weak_ptr<Synapse>`; a pruned synapse expires and falls
back to the scan rather than being resurrected.

## 4. What still blocks the learning

Measured by isolation rather than inference, at `--steps=20`:

| configuration | wall |
|---|---|
| closed loop, no eligibility | **3 s** |
| eligibility only, no closed loop | 200 s (timeout) |
| both | 200 s (timeout) |

The reward loop is not the cost. Eligibility accumulation is, and on its own.

What remains per spike, after the three fixes: a region walk with O(1) lookups,
one neuron-mutex acquisition, and then **~128 map updates under the single global
`syn_state_mutex_`**. Every spike from every neuron serialises on that one lock.
At roughly a million spikes per run that is on the order of 10^8 mutex-protected
map operations.

This is structural, not a constant factor. Fixing it means changing how
eligibility is represented — a flat array indexed by synapse id instead of a
hashed map, per-neuron accumulation expanded lazily at reward time, or sharded
locks. That is a larger piece of work than this pass, and guessing at further
micro-optimisations was already the wrong instinct twice.

## 5. Honest status

- The sensorimotor loop is **built and running**, and its baseline is measured.
- Reward is **computed and delivered** correctly, via `deliverReward`.
- Phase-4 plasticity is **reachable in principle** and **not runnable in
  practice**, for a reason that is now located precisely rather than suspected.
- The agent therefore still cannot learn. Nothing in this note should be read as
  saying it does.

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Non-autonomous anatomical brain still exact: Thalamus 0.6955 x 3.
- n=8192 still builds and runs: 1,048,576 synapses, 21 s for 5 steps.
- Full build: 106/106, 0 errors.
- Test sweep: 33 pass, 3 fail — the pre-existing baseline.

## Next

1. Re-represent eligibility so accumulation is not O(spikes x synapses) under a
   global lock. Until that lands, Phase-4 cannot be exercised at this scale and
   no claim about reinforcement learning in this codebase can be tested.
2. Then re-run the phototaxis task and compare mean distance against the 1.81
   baseline recorded here, with within-condition variance measured first.
