# Making the connectome grow, and the anatomical regions run

Date: 2026-08-23
Follows: `connectome_plasticity_audit_2026-08-23.md`, which found the connectome
structurally frozen and the fourteen region classes never constructed.

## Result

Both are now reachable and both demonstrably work.

| | before | after |
|---|---|---|
| synapses at 100 / 400 / 800 steps | 102 / 102 / 102 | 102 / 117 / 117 |
| regions constructed | 2 generic | 11 anatomical subclasses |
| neuron activity, anatomical brain | — | 512/704 = **72.7%** |

The default path is unchanged: 148,604 total updates and 102 active synapses,
identical to before this work. Both features are opt-in.

## 1. Structural plasticity is now reachable

New flags:

```
--structural-plasticity[=on|off]
--structural-grow-batch=N
--structural-spawn-batch=N
--structural-prune-threshold=F     (0..1)
--structural-interval=N
--structural-energy-gate=F         (0..1)
```

Enabling the feature now also supplies live values, because the defaults could
not produce growth on their own:

```cpp
if (structural_set && lconf.enable_structural_plasticity) {
  if (!structural_grow_set  && lconf.structural_grow_batch == 0)        lconf.structural_grow_batch = 8;
  if (!structural_prune_set && lconf.structural_prune_threshold <= 0.f) lconf.structural_prune_threshold = 0.05f;
  if (lconf.structural_max_regions_per_cycle < 2) lconf.structural_max_regions_per_cycle = 16;
}
```

`structural_grow_batch` and `structural_spawn_batch` both default to 0 while
`applyStructuralPlasticity` guards each with `if (batch > 0)`, so
`enable_structural_plasticity = true` on its own grew nothing and spawned
nothing — only pruning ran. `structural_max_regions_per_cycle` defaults to 1,
which changes one region per cycle and is hard to distinguish from no change.
The substitution is printed:

```
[Learning] structural plasticity ON grow=8 spawn=0 prune_thr=0.05 interval=100 regions/cycle=16
```

`spawn` (neurogenesis) is deliberately left at 0 — it allocates neurons rather
than synapses and should be opted into explicitly.

### Every knob is causal

Measured at 600 steps:

| configuration | synapses |
|---|---|
| off | 102 |
| grow=8 | 118 |
| grow=32 | 162 |
| grow=32, interval=25 | 307 |
| grow=32, interval=25, prune=0.5 | 102 |

The last row is the useful control: aggressive pruning cancels aggressive growth
exactly back to baseline, so growth and pruning are both live and opposed.

### A design bug found while verifying: synapses were born condemned

`growSynapses` defaults `initial_weight` to 0.05f and `structural_prune_threshold`
also defaults to 0.05f. Every new synapse was created at **exactly** the weight
that makes it eligible for deletion, so survival turned on whether learning
nudged it up or down by an epsilon before the next prune — and a new synapse
never got a chance to strengthen before being judged.

Measured at 400 steps with growth always permitted: prune threshold 0.05 (equal
to birth weight) gave 130/131 synapses; threshold 0.01 gave 133/134. New
synapses were being deleted at birth.

`applyStructuralPlasticity` now gives newborn synapses headroom:

```cpp
const float birth_weight = (config_.structural_prune_threshold > 0.0f)
                             ? config_.structural_prune_threshold * 2.0f : 0.05f;
region->growSynapses(config_.structural_grow_batch, 0.6f, birth_weight);
```

This also removed most of the run-to-run variation, because the knife-edge
comparison was itself the divergence point.

### Determinism: fixed at default settings, NOT at aggressive ones

| configuration | 6 runs |
|---|---|
| structural off | 102 x 6 |
| `--structural-plasticity` | 117 x 6 |
| `--structural-plasticity --structural-energy-gate=0.0` | 131 x 6 (was 129/130/131/132) |
| `grow=32 --structural-interval=25` | **306 / 307 / 309** |

So a residual nondeterminism remains under high synapse-creation rates. What is
known about it:

- The baseline without structural plasticity is exact (148,604 updates x 4), so
  the source is in the growth path, not upstream.
- With growth disabled but pruning active (`--structural-energy-gate=1.0`) it is
  exact (102 x 6). Pruning alone is deterministic.
- One structural cycle is exact (118 x 5); two are exact (133 x 5); divergence
  appears from roughly four cycles on.

The most likely remaining candidate is the other float boundary in
`growSynapses`: eligible neurons are selected by `getActivation() >=
min_activation` (0.6f), so a neuron sitting near 0.6 can enter or leave the
`active` set, changing the shuffle and therefore which pairs get connected.
Not yet confirmed — recorded as open rather than claimed fixed.

**Practical guidance:** paired comparisons involving structural plasticity should
use default growth rates, where runs are currently exact, and should verify
determinism for their own configuration before relying on it.

## 2. The anatomical regions now run

`RegionFactory::createRegion` always returned a base `Region`, so none of the
fourteen implemented subclasses was ever constructed. Added:

```cpp
static void setAnatomicalDispatch(bool enabled, std::size_t neuron_count = 64);
static bool anatomicalDispatchEnabled();
static bool isAnatomicalName(const std::string& name);
```

When enabled, `createRegion(name, ...)` resolves the name to its subclass by
lowercased substring, most specific first (so "prefrontalcortex" does not match
on "cortex", and "somatosensorycortex" does not match on "sensory"). The subclass
assigns its own id and sets the Type and ActivationPattern of the real structure
— Hippocampus is Subcortical/Oscillatory — so the type requested by the caller is
deliberately overridden where an implementation exists.

**Off by default, for two concrete reasons:** the subclass constructors call
`createNeurons()` with defaults up to 1,000,000 (Hippocampus 1M, Thalamus 800k,
Amygdala 500k), and they change behaviour by running their own `process()`. The
neuron count is therefore overridden via `--anatomical-neurons=` (default 64).

`--anatomical-regions` builds a brain of eleven regions wired to coarse anatomy
rather than all-to-all: sensory cortices report to the thalamus, thalamus relays
to prefrontal, hippocampus and amygdala exchange with prefrontal, prefrontal
drives motor, motor drives brainstem. It is a sketch, not a connectome, but it is
directed and it is not arbitrary.

```
[Anatomy] VisualCortex neurons=64      ... 11 regions ...
  Total Updates: 846766
  Active Synapses: 2470
  Total Neurons: 704
  Active Neurons: 512 (72.7%)
```

The activity figure is the interesting one. The generic demo brain saturates at
**100%** active — every neuron over threshold, which carries no information. The
anatomical brain sits at **72.7%**, i.e. the region classes produce differentiated
activity. That is a behavioural difference, not just a naming one, and it is the
first evidence in this codebase that the region implementations do something.

Determinism: 846,766 total updates across 4 runs, exact.

Composes with structural plasticity: 2,470 synapses at 120 steps, 2,510 with
`--structural-plasticity`, 2,509 at 400 steps — it grows, then prunes.

## Verification

- Default path unchanged: 148,604 updates / 102 synapses / 64 neurons at 100%.
- Full build: 106/106 targets, 0 errors.
- Test sweep: 33 pass, 3 fail — same as the pre-existing baseline
  (`test_memorydb`, `test_phase2_memory`,
  `test_substrate_language_integration`).

## Still open

1. **Nondeterminism under aggressive growth**, above. Suspect the
   `min_activation` boundary in `growSynapses`.
2. **Neurogenesis is untested** — `--structural-spawn-batch` exists and is wired,
   but is left at 0 by default and has not been exercised here.
3. **Three region classes are still unreachable** by name from the demo brain:
   `DefaultModeNetwork`, `SelfNode`, `PhaseAMimicryRegion`. Dispatch supports the
   first two; they are simply not in the eleven-region set.
4. **Axonal retraction and myelination remain absent**, as does developmental
   staging. Growth and pruning now exist; the connectome can gain and lose
   synapses but not re-route long-range tracts or vary conduction delay.
