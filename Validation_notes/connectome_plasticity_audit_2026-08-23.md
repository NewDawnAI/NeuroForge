# Is the connectome in NeuroForge actually growing?

Date: 2026-08-23
Question: does this codebase implement a lifelong-plastic connectome, does it
have the brain's regions with their real functions, and where would HDC fit?

## Summary

The connectome is **structurally frozen at runtime**. Measured: the default
brain reports 102 active synapses at 20, 100 and 400 steps — the count never
moves, at any horizon.

The machinery for growth and pruning exists and is well written. It is switched
off by default, has no command-line surface at all, and two of its three batch
sizes default to zero, so enabling it would still not grow anything.

The fourteen anatomically-named region classes exist and compile, but nothing
constructs them.

## 1. Against the biological picture

| mechanism | in NeuroForge | executes by default |
|---|---|---|
| Synaptic strengthening (Hebbian/STDP) | yes | **yes** — 67,004 updates at 200 steps |
| Homeostatic scaling | yes | only with `--homeostasis` (fixed 2026-08-23) |
| Synaptic pruning | `Region::pruneWeakSynapses` | **no** — gated off |
| Synaptogenesis / axonal sprouting | `Region::growSynapses` | **no** — gated off *and* batch = 0 |
| Neurogenesis | `Region::spawnNeurons` | **no** — gated off *and* batch = 0 |
| Axonal retraction | absent | — |
| Myelination | absent (0 references) | — |
| Cortical remapping | absent (0 references) | — |
| Neuromodulator routing | 4 constants, dead class | **no** |
| Developmental lifecycle | absent | — |

So of the three scales in the biological account, only **microscale
strengthening** is live. Microscale pruning is written but disabled; macroscale
structural rewiring is partly written (growth) and partly absent (retraction,
myelination, remapping); functional neuromodulatory routing is effectively
absent.

## 2. Why structural plasticity never runs

Entry guard, `LearningSystem.cpp:320`:

```cpp
if (config_.enable_structural_plasticity && brain_) {
```

Defaults, `include/core/LearningSystem.h:107-113`:

```cpp
bool        enable_structural_plasticity = false;
float       structural_prune_threshold   = 0.05f;
std::size_t structural_spawn_batch       = 0;   // neurogenesis
std::size_t structural_grow_batch        = 0;   // synaptogenesis
float       structural_energy_gate       = 0.5f;
std::size_t structural_interval_steps    = 100;
std::size_t structural_max_regions_per_cycle = 1;
```

And inside `applyStructuralPlasticity`:

```cpp
if (config_.structural_spawn_batch > 0) { region->spawnNeurons(...); }
if (config_.structural_grow_batch  > 0) { region->growSynapses(...); }
```

Two separate reasons nothing happens:

1. `enable_structural_plasticity` is false, and **there is no CLI flag for it** —
   `grep '"--structural'` in `main.cpp` returns nothing. It cannot be turned on
   without editing code.
2. Even set true, both batch sizes are 0, so the two growth paths are skipped.
   Only pruning would run.

This is the **third instance this session** of the same defect shape: a flag that
initialises a subsystem with parameters that disable it.

- `--enable-learning` with `hebbian_rate`/`stdp_rate` at 0.0 → 0 updates
  (fixed 2026-08-22)
- `--homeostasis` with `homeostasis_eta` at 0.0 → early return every step
  (fixed 2026-08-23)
- `enable_structural_plasticity` with both batches at 0 → growth skipped
  (this note)

Worth treating as a class of bug rather than three incidents. A config whose
"enabled" state still needs a second non-default parameter to do anything should
either default that parameter to something live, or refuse to start.

The gating itself is thoughtful and worth keeping: growth is held back under low
mitochondrial energy or high metabolic stress, while pruning runs regardless —
which is a defensible reading of the biology. The problem is reachability, not
design.

## 3. The regions exist but are never constructed

Fourteen region classes are defined with real `process()` overrides:

Amygdala, AuditoryCortex, Brainstem, CingulateCortex, DefaultModeNetwork,
Hippocampus, Insula, MotorCortex, PhaseAMimicryRegion, PrefrontalCortex,
SelfNode, SomatosensoryCortex, Thalamus, VisualCortex.

None is instantiated anywhere in `src/`. The reason is a single line —
`RegionFactory::createRegion` in `Region.cpp`:

```cpp
return std::make_shared<Region>(id, name, type, pattern);
```

It always returns the **base** `Region`. There is no name- or type-based dispatch
to any subclass, so `createRegion("Hippocampus", ...)` yields a generic region
that happens to be called Hippocampus. `Region::Type` has only five values
(Cortical, Subcortical, Brainstem, Special, Custom) and carries no
region-specific behaviour.

The `NF_ForceLink_*` functions called at startup are empty:

```cpp
extern "C" void NF_ForceLink_CorticalRegions() {}
```

They exist only to stop the linker discarding the translation units — which is
itself the tell that nothing references those symbols.

Net: the anatomy is present as a **taxonomy**, not as a functioning set of
differentiated regions. The default run builds two generic regions named
DemoCortex and DemoSubcortex.

Neuromodulators are in the same position. `SubcorticalRegions.cpp:554-557` sets

```cpp
neurotransmitter_levels_["dopamine"]      = 0.5f;
neurotransmitter_levels_["serotonin"]     = 0.5f;
neurotransmitter_levels_["norepinephrine"]= 0.5f;
neurotransmitter_levels_["acetylcholine"] = 0.5f;
```

— four constants, in a class nothing constructs.

## 4. HDC in NeuroForge

Nothing today: `hypervector`, `hyperdimensional`, `VSA`, `bipolar`,
`holographic`, bind/unbind and circular convolution all return **0 matches**.

Before adding it, the relevant result is already in hand from the sibling RDLNN
study (`Architecture_Substrate_Research/`, Aug 2026), and it cuts both ways:

- The architecture is **viable but N-inefficient, not impossible**. The earlier
  headline that reservoir state space was ~19-dimensional and therefore killed
  the HD premise was **retracted** — usable capacity does grow strongly with N;
  participation ratio simply fails to measure it.
- Capacity is a **sharp threshold**, not a scaling law: top-1 retrieval needs
  d² ≍ n log n (Barnfield et al., arXiv:2605.05189). Under-provision the
  dimension and it fails abruptly rather than degrading.
- HDC cleanup is **not a separate mechanism** — it is modern Hopfield retrieval
  in the β→∞ limit (Millidge et al., *Universal Hopfield Networks*, ICML 2022).
  Adding "HDC" alongside an attention/energy retrieval step adds a name, not a
  capability.
- The **β-confound rule** applies to any comparison: sweep the free parameter
  before attributing an effect to the thing you changed. In the RDLNN study an
  apparent win for mean-centring was a temperature effect; at 2× β every arm
  reached 1.000 retrieval, including the unnormalised baseline.

Where it would genuinely pay here is the memory problem this codebase now has,
not representation for its own sake. Measured 2026-08-23: **657 bytes per
synapse**, giving ~36 GB at fruit-fly scale (~54.5M synapses). A hypervector
substrate replaces per-synapse `shared_ptr` graphs with fixed-width vectors and
arithmetic, which is a different memory regime entirely.

Note also that the fly circuit the scale target comes from is itself the
canonical HDC-adjacent result: divisive normalisation → sparse binary random
projection → winner-take-all (Dasgupta, Stevens & Navlakha, *Science* 358:793,
2017). If the goal is fly-scale, that is the published blueprint.

## 5. What would make the connectome actually grow

In dependency order, cheapest first:

1. **Reachability.** Add `--structural-plasticity`, `--structural-grow-batch=`,
   `--structural-prune-threshold=`, `--structural-spawn-batch=`, and default the
   batches to something non-zero when the feature is enabled. Nothing else on
   this list can be measured until this exists.
2. **Verify it moves the number.** With reporting already in place, a growing
   connectome should show `Active Synapses` changing across steps. Today it is
   constant at 102, which is the check that would catch a regression.
3. **Region dispatch.** Make `RegionFactory` return the right subclass by
   type/name, so the fourteen implemented regions actually run. This is the
   single highest-leverage change for "real regions with real functions".
4. **Retraction** to complement growth, so the connectome can lose long-range
   links and not only gain them.
5. **Myelination** as a per-axon conduction-delay term — meaningful only once
   spike timing matters, so it should follow, not lead.

Items 1-3 are reachability and wiring problems on code that already exists.
Items 4-5 are new mechanism.

## Method note

Everything above distinguishes "code exists" from "code executes". The
step-count sweep (102 synapses at 20/100/400 steps) is the behavioural evidence;
the greps and default values are the mechanism. That distinction is what this
session repeatedly found to matter — static presence of a subsystem says nothing
about whether it runs.
