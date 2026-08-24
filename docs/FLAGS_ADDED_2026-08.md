# Flag reference: determinism, plasticity, anatomy, closed loop

Added 2026-08-22 → 2026-08-24. None of these existed before; none was documented
until this file.

All are **opt-in**. The default invocation is unchanged and still produces
148,604 total updates / 102 active synapses at `--steps=400 --enable-learning
--phase-c-seed=401 --sequential`, which is the regression check to run after
touching any of this.

---

## Determinism

| flag | effect |
|---|---|
| `--sequential` / `--sequential=off` | Process regions in a fixed order instead of `std::execution::par_unseq`. **Required for bit-exact runs.** Without it the residual spread is 8 on 67,000 (0.012%). |
| `--autonomous-sync` | Run the autonomous loop inline instead of on its own thread. |

`--autonomous-sync` matters more than it looks. The threaded autonomous loop
races the main step loop, so how many cycles interleave is the scheduler's
choice. Measured at `--steps=60`, Thalamus mean activation across three identical
invocations: **0.6955 / 0.6955 / 0.6955** without autonomous mode, and
**0.8836 / 0.8835 / 0.8809** with the thread. Any decision-level claim measured
under the threaded form is unreproducible; use `--autonomous-sync` for those.

The threaded path remains the default so existing behaviour is untouched.

Also relevant: `--phase-c-seed=N` now actually reaches everything.
`DeterministicRng::seedFor("Site")` replaced 15 `std::random_device` seedings and
one `high_resolution_clock` seeding. Before that, four identical invocations gave
44,660 / 49,064 / 47,718 / 47,252 total updates.

---

## Network size

| flag | default | effect |
|---|---|---|
| `--demo-neurons=N` | 32 | Neurons per region in the default two-region brain. |
| `--demo-density=F` | 0.05 | Connection probability between them. |

These replaced hardcoded literals. The old values made the synapse count a fixed
arithmetic identity — `32 × 32 × 0.05 × 2 = 102` — which is why every run
reported exactly 102 active synapses and why the number never moved.

Measured range:

| neurons/region | synapses | fan-in |
|---|---|---|
| 32 | 102 | 1.6 |
| 256 | 32,768 | 64 (cap) |
| 1024 | 131,072 | 64 (cap) |
| 8192 | 1,048,576 | 64 (cap) |

`max_per_source_cap = 64` in `HypergraphBrain::connectRegions` bounds fan-in, so
density stops mattering above `64/neurons`. At n=8192 that is 16,384 neurons and
1,048,576 synapses in 73 s — a configuration that previously exceeded 900 s and
was killed.

Memory is the binding constraint at scale: **~657 bytes per synapse**, linear.
Fly scale (~54.5M synapses) projects to ~36 GB.

---

## Structural plasticity

| flag | default when enabled | effect |
|---|---|---|
| `--structural-plasticity[=on\|off]` | off | Master switch for growth, pruning, neurogenesis. |
| `--structural-grow-batch=N` | 8 | Synapses grown per region per cycle. |
| `--structural-spawn-batch=N` | 0 | Neurons spawned per region per cycle (neurogenesis). |
| `--structural-prune-threshold=F` | 0.05 | Weight below which a synapse is removed. |
| `--structural-interval=N` | 100 | Steps between structural cycles. |
| `--structural-energy-gate=F` | 0.5 | Mitochondrial energy required before growth. Pruning ignores this. |

Enabling the master switch also supplies live batch values, because
`structural_grow_batch` and `structural_spawn_batch` both default to **0** while
`applyStructuralPlasticity` guards each with `if (batch > 0)`. Setting
`enable_structural_plasticity = true` alone grew nothing and spawned nothing —
only pruning ran. `structural_max_regions_per_cycle` is also widened from 1,
which touches one region per cycle and is hard to distinguish from no change.

Every knob is causal, measured at 600 steps:

| configuration | synapses |
|---|---|
| off | 102 |
| `grow=8` | 118 |
| `grow=32` | 162 |
| `grow=32 --structural-interval=25` | 307 |
| the same plus `--structural-prune-threshold=0.5` | 102 |

That last row is the control worth keeping: aggressive pruning cancels aggressive
growth exactly back to baseline, so both directions are live and opposed.

**Determinism caveat.** Exact at default rates (117 × 6 runs), and **not** exact
at aggressive ones (`grow=32 --structural-interval=25` gives 306/307/309). The
baseline without structural plasticity is exact and pruning alone is exact, so
the residue is in the growth path — most likely the `activation >= 0.6f`
membership test in `growSynapses`. Recorded as open, not fixed.

`--structural-spawn-batch` is left at 0 by default because it allocates neurons
rather than synapses, and it has **not been exercised** by any measurement here.

---

## Anatomical regions

| flag | default | effect |
|---|---|---|
| `--anatomical-regions` | off | Build a 12-region brain from the implemented region classes instead of two generic ones. |
| `--anatomical-neurons=N` | 64 | Neurons per anatomical region. |

Fourteen region classes (Hippocampus, Amygdala, PrefrontalCortex, Thalamus,
VisualCortex, MotorCortex, Brainstem, Insula, CingulateCortex,
DefaultModeNetwork, SelfNode, AuditoryCortex, SomatosensoryCortex,
PhaseAMimicryRegion) were implemented with real `process()` overrides and **none
was ever constructed**: `RegionFactory::createRegion` always returned a base
`Region`. The `NF_ForceLink_*` calls at startup are empty function bodies whose
only purpose is stopping the linker discarding those objects.

`--anatomical-neurons` exists because the subclass constructors default to very
large counts — Hippocampus 1,000,000, Thalamus 800,000, Amygdala 500,000.

Wiring follows coarse anatomy rather than all-to-all: sensory cortices → thalamus
→ prefrontal; hippocampus and amygdala ↔ prefrontal; prefrontal → motor →
brainstem. A sketch, not a connectome, but directed and not arbitrary.

```
12 regions, 768 neurons, 2,876 synapses, 642 active (83.6%)
```

The generic demo brain saturates at **100%** active — every neuron over
threshold, carrying no information. The anatomical brain does not, which is the
first behavioural evidence that the region implementations do something.

Off by default because it changes both allocation and behaviour.

---

## Substrate ablation

| flag | effect |
|---|---|
| `--ablate=none\|zero\|noise\|shuffle` | Corrupt region activations at the end of each step. |
| `--ablate-region=SUBSTR` | Restrict ablation to regions whose name contains SUBSTR. Empty = all. |
| `--ablate-seed=N` | Seed for the ablation's private generator. |

`shuffle` is the sharpest form: it permutes activations across neurons, so the
value distribution is bit-identical and only *which neuron carries what* is
destroyed. A knockout flag cannot separate "unused" from "redundantly covered"
because it changes both signal and magnitude; shuffle can. The generator is
private and explicitly seeded so ablation never perturbs the global stream.

---

## Homeostasis

| flag | effect |
|---|---|
| `--homeostasis[=on\|off]` | Enable homeostatic synaptic scaling. |
| `--homeostasis-eta=F` | Scaling rate, `[0,1]`. Defaults to 0.01 when `--homeostasis` is given bare. |

`homeostasis_eta` declares as `0.0f` and `applyHomeostasis()` opens with
`if (!enabled || eta <= 0.0f) return;`, so `--homeostasis` alone set the enable
bit and returned immediately every step. Measured at 200 steps: avg weight change
5.38e-05 with the flag against 5.27e-05 without it (noise), versus 5.70e-04 with
`--homeostasis-eta=1.0`.

**What homeostasis does not do:** change the synapse count, which is structural,
or the active-neuron fraction, which is already saturated. Tested at 20 steps
where activity has genuine headroom (7.8%), homeostasis changed average weight
change by **7,600×** and left both the synapse count and the active-neuron count
bit-identical. A well-powered null, not an untested assumption.

---

## Closed sensorimotor loop

| flag | default | effect |
|---|---|---|
| `--sensory-drive[=on\|off]` | off | Inject a moving checker grid into the anatomical `VisualCortex` each step. |
| `--closed-loop` | off | Phototaxis task: the brain's decision moves an agent, and the resulting brightness change returns as reward. |
| `--learn-policy` | off | Use a learned softmax policy instead of the hardcoded argmax. |
| `--policy-lr=F` | 0.05 | REINFORCE learning rate. |
| `--policy-temp=F` | 1.0 | Softmax temperature; higher explores more. |

`--closed-loop` supersedes `--sensory-drive` when both are given: the checker
pattern has identical mean brightness at every shift, so no action can change any
objective computed from it.

The loop runs sense → decide → act → consequence → reward → update. The hook
fires at the *top* of a cycle, so it applies the previous cycle's decision, scores
the outcome, then presents the new observation. That one-cycle delay is what
eligibility traces are for.

### Result

1,500 steps, 3 runs per condition, mean distance to the light on an 8-ring
(random walk ~2.0, on the light 0):

| | run means | overall | spread |
|---|---|---|---|
| fixed argmax | 2.18, 1.75, 2.48 | 2.139 | 0.73 |
| `--learn-policy` | 0.92, 0.98, 1.00 | **0.967** | 0.08 |

Effect +1.172, larger than the largest within-condition spread, with no overlap.
Learning curve in every run from uniform weights: `1.67 → 0.60 → 0.80 → 0.60`.

**What this is not:** the substrate learning. The weights that moved are in
`PrefrontalCortex`, not the connectome. Phase-4 reward-modulated plasticity fires
(237,409 updates) and has no measurable behavioural effect, because uniform
eligibility broadcasts reward to everything recently active. A policy learner was
added *alongside* the biological substrate.

---

## Substrate credit assignment

| flag | effect |
|---|---|
| `--action-credit` | Confine eligibility to the motor channel of the selected action. |
| `--motor-selection` | Select among MotorCortex action channels rather than input regions. |

Both are opt-in and both are **necessary but not sufficient** for the substrate
to learn. The current state, measured:

| | overall mean distance |
|---|---|
| uniform credit | 1.961 |
| gated credit | 1.950 |
| *(learned policy layer, for reference)* | *0.967* |

Effect +0.011 against a within-condition spread of 0.317 — a null.

Three defects were removed to get here, in order: eligibility was a flat 0.1
bump over every synapse of every spiking neuron (now three-factor,
`rate × pre × post`); the traces never decayed, so every synapse that ever fired
sat at the cap forever and the trace carried no timing information (now λ=0.85
per step); and credited synapses were not on the decision path at all, since
selection read Thalamus/Hippocampus/Amygdala/CingulateCortex while credit landed
on MotorCortex (now `--motor-selection`).

**A reward baseline was tried and is a null** (`--reward-baseline`,
`--reward-baseline-rate=`): +0.006 against a within-condition spread of 0.367.
Two reasons, and the first invalidates the reasoning that motivated it.

`(1[a==chosen] − p[a])·x` in the learned policy is **∇log π for a softmax** — the
score function, not a baseline. That update is `lr·R·∇log π`, i.e. REINFORCE, a
policy gradient with no baseline in it. The substrate's `κ·R·eligibility`
multiplies reward by a *coincidence* trace, which is reward-modulated Hebbian and
does not perform gradient ascent on expected reward. No baseline converts one
into the other.

Separately, an EMA baseline converges to the mean of R, and here R is a
brightness *change* with mean ≈ 0, so `R − b ≈ R`. Measured at 400 steps, the
potentiated/depressed ratio moved 1.554 → 1.538 with the flag confirmed active —
a no-op, which is why the behavioural null was predicted rather than discovered.

**What would actually work:** a state-dependent critic V(s), so the modulator is
TD error `R + γV(s′) − V(s)` and stays informative when raw reward is mean-zero;
or an eligibility trace that approximates ∇log π, e.g. node perturbation, where
the trace becomes `(actual − expected activation) × input`. The second preserves
locality, which is this project's architectural commitment.

Superseded text follows for the record:

~~What remains is a reward baseline.~~ `dw = kappa·R·eligibility` has none, and
reward here is a change in brightness with a mean near zero, so updates alternate
sign and cancel — measured as potentiated 5.5–5.9M against depressed 4.1–4.5M,
nearly balanced, in every run. The policy layer succeeds on the *same* reward
because its update carries an advantage term, `(1[a==chosen] − p[a])`.

Biologically this is the pre-Schultz model: dopamine encodes reward *prediction
error*, not reward. Adding `dw = kappa·(R − R̄)·eligibility` with a running mean
is the next step.

### Selection smoothing

| flag | effect |
|---|---|
| `--selection-smoothing=F` | EMA over each action channel before the argmax. 0 = off. |

Smooths the **decision variable**, not the substrate — activations and plasticity
are untouched. The mechanism works: channel spread 0.65 → 0.424 at α=0.10, and
decision changes fell from ~400 to 40 per run.

**A null, and conclusively so.** Three levels: no smoothing 1.944, α=0.10 2.022
(−0.078), α=0.02 1.889 (+0.056), against a max within-condition spread of 0.283.
Both inside the noise and **pointing in opposite directions**. If selection-point
noise were the binding constraint, α=0.02 — averaging five times harder — should
have helped proportionally more. The absence of dose-response is what makes this
a conclusion rather than another inconclusive arm.

### Critic and node perturbation

| flag | effect |
|---|---|
| `--critic` | Learn V(s) online; modulate plasticity by TD error `R + γV(s′) − V(s)` instead of raw reward. |
| `--critic-lr=F` | Critic learning rate (default 0.05). |
| `--critic-gamma=F` | Discount on the successor state's value (default 0.9). |
| `--node-perturbation` | Eligibility becomes `rate × pre × (post − E[post])` — a local gradient estimator rather than a coincidence measure. |

Both engage and **both are nulls** on behaviour: +0.033 against a
within-condition spread of 0.483.

The manipulation check passes, which is what makes this informative rather than
inconclusive — potentiated/depressed ratio, control 1.303/1.315/1.348 against
critic+NP 1.242/1.251/1.239, **no overlap**. The mechanisms change the update
statistics reliably; they do not change behaviour.

**Where the chain breaks.** Reward reaches the weights (~2.5M updates/run), the
mechanisms change the updates, and the motor channels differentiate strongly
(spread ~0.65). But that differentiation does **not** grow with learning (0.627
early → 0.685 late) and selection stays uniform (~25% per action, early and
late). Intrinsic channel fluctuation swamps a per-update weight change of order
1e-4, so the learned component never rises above the noise at the argmax.

That is a signal-to-noise problem at the selection point, not a missing
mechanism. The policy layer solves the same task with the same reward because its
parameters sit *directly* at the decision (`score[a] = w[a]·x`), so a weight
change **is** a change in the decision.

**Traces are now signed.** Node perturbation yields negative eligibility when a
neuron fires below its expectation, and that sign is the information. `bump()`
clamped to `[.., cap]` and `decay()` tested `next < floor`, which would have
zeroed every negative trace on its first decay — the null would have been
measured with half the signal discarded and nothing to indicate it.

## Eligibility and Phase-4

| flag | effect |
|---|---|
| `--auto-eligibility[=on\|off]` | Accumulate per-synapse eligibility traces from spikes. Required for Phase-4 to do anything. |

`auto_eligibility_accumulation_enabled_` defaults to false, so eligibility never
accumulated and `dw = κ·R·eligibility` was always 0 — which is why Phase-4
reported **0 updates** indefinitely. With it on, 237,409.

This flag also exposed a **latent deadlock**, not a slowness: `Region::getNeuron`
took `region_mutex_`, and neuron spike callbacks fire from code already holding
it. `std::mutex` is not recursive, so the lookup deadlocked against its own
caller on the first spike. `neuron_index_` now has its own mutex.

---

## Reporting added

Not flags, but new output that the measurements above depend on:

- `Total Neurons` / `Active Neurons (%)` — neuron activity was computed in
  `Region::updateStatistics` and never printed.
- `Regions:` — a per-region breakdown of neuron count, active count and mean
  activation. Aggregates hide *where* signal lives, which is what tracing
  propagation needs.
- `[Integration] <Region>: ACTIVE | INERT` — whether the downcast an integration
  depends on actually succeeded. It used to fail silently.
- `[Decision] PFC option N of M conf=… norm=[…] raw=[…]` — printed only when the
  choice changes. A decision driven by state should move as state moves; one that
  never moves is the signature of constant inputs.
- `[World] iter=… agent=… dist=… brightness=…` — closed-loop task state.
- `[Sensory] … drive INERT` — when the sensory drive cannot reach a region.

---

## Useful invocations

Regression check after any change:

```
--steps=400 --enable-learning --phase-c-seed=401 --sequential
```

Expect 148,604 total updates and 102 active synapses.

A growing connectome:

```
--steps=600 --enable-learning --phase-c-seed=401 --sequential --structural-plasticity
```

The 12-region anatomical brain:

```
--steps=120 --enable-learning --phase-c-seed=401 --sequential --anatomical-regions
```

The full closed loop with learning:

```
--steps=1500 --enable-learning --phase-c-seed=401 --sequential \
  --anatomical-regions --autonomous-mode --autonomous-sync \
  --enable-pfc --enable-motor-cortex --closed-loop \
  --auto-eligibility=on --learn-policy
```
