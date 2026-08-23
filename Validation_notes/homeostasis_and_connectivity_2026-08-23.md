# Can homeostasis make all synapses and neurons active?

Date: 2026-08-23
Binary: `build/neuroforge.exe`, mingw64, rebuilt this session
Invocation base: `--enable-learning --phase-c-seed=401 --sequential`

## Short answer

No, and for two different reasons — one per half of the question.

- **Synapses:** "active synapses" is not an activity measure. It is a count of
  synapses that exist. Homeostasis cannot move it, because homeostasis only
  rescales the weights of synapses already present. The lever is construction,
  and construction moves it a long way: 102 -> 131,072 measured.
- **Neurons:** neuron activity *is* a real threshold measure, and it does not
  need homeostasis. The network saturates to 100% on its own. Homeostasis was
  measured to have no effect on it even when its effect on weights was 7,600x.

A separate defect was found and fixed along the way: `--homeostasis` on its own
did nothing at all.

## 1. "Active Synapses" counts existence, not activity

`LearningSystem.cpp:982-992` sums `getInputSynapseCount()` over every neuron of
every region. Nothing is compared against a threshold. So the reported number is
the size of the synapse set.

This explains a number that had been stable all session. The default brain is
built by `create_demo_brain` (`main.cpp` ~2552) as two regions of 32 neurons
wired at density 0.05:

```
32 sources x 32 targets x 0.05 x 2 directions = 102.4
```

Measured: **102**. The count never moved because nothing in the system moves it.

## 2. Homeostasis cannot create synapses

`LearningSystem.cpp:684-700` iterates `getRegionSynapses(region_id)` and calls
`setWeight()` on each. It is a rescaling rule over an existing set:

```cpp
float homeostatic_adjustment = config_.homeostasis_eta * (1.0f - avg_activation);
```

There is no allocation path. This is structural, not a tuning question.

Note the set-point: the adjustment vanishes at `avg_activation == 1.0`, so this
rule drives average activation *toward* maximum. Biological homeostatic
plasticity targets a low sparse firing rate; this targets saturation. Recorded
as an observation, not changed.

## 3. Construction is the lever, and it works

Added `--demo-neurons=` and `--demo-density=` so the default brain's size is a
runtime knob rather than two hardcoded literals. Measured synapse counts:

| neurons/region | density | total neurons | synapses | fan-in per neuron |
|---|---|---|---|---|
| 32 | 0.05 | 64 | 102 | 1.6 |
| 32 | 0.5 | 64 | 1,024 | 16 |
| 256 | 0.1 | 512 | 13,086 | 25.6 |
| 256 | 0.5 | 512 | 32,768 | 64 (cap) |
| 1024 | 0.1 | 2,048 | 131,072 | 64 (cap) |

A 1,285x range. The last two rows sit exactly on `max_per_source_cap = 64` in
`HypergraphBrain::connectRegions`, so synapses per direction saturate at
`neurons * 64` regardless of density. Reaching fruit-fly mean fan-in (~350)
requires raising that cap, not raising density.

## 4. Neuron activity is real, and saturates without help

`Region.cpp:844-858` counts a neuron active when `activation > 0.2f`. Added a
readout for it — it was computed but never printed.

Connectivity alone drives it, at 20 steps:

| config | synapses | active neurons |
|---|---|---|
| n=32 d=0.05 | 102 | 5 / 64 (7.8%) |
| n=256 d=0.5 | 32,768 | 512 / 512 (100%) |
| n=1024 d=0.1 | 131,072 | 2,048 / 2,048 (100%) |

At the default size, 200 steps reaches 100% with homeostasis **off** — the 7.8%
at 20 steps is warm-up transient, not a homeostatic deficit.

## 5. The null, with its manipulation check

Tested at 20 steps, where activity has real headroom (7.8%), so a null cannot be
dismissed as ceiling effect:

| homeostasis | synapses | avg weight change | active neurons |
|---|---|---|---|
| off | 102 | 5.37e-07 | 5 (7.8%) |
| on (eta 0.01) | 102 | 4.09e-03 | 5 (7.8%) |
| eta 0.5 | 102 | 6.18e-03 | 5 (7.8%) |

The manipulation check passes hard: homeostasis changes average weight change by
**7,600x**. Over that range the synapse count and the active-neuron count are
both bit-identical. This is a well-powered null, not an untested assumption.

(eta is validated to [0,1]; eta=2.0 is rejected with a clear message.)

## 6. Defect found and fixed: `--homeostasis` was a silent no-op

`LearningSystem::Config` declares `homeostasis_eta = 0.0f`, and
`applyHomeostasis()` opens with:

```cpp
if (!config_.enable_homeostasis || config_.homeostasis_eta <= 0.0f) return;
```

So `--homeostasis` set the enable bit and then returned every step. Measured
before the fix, at 200 steps:

| flags | avg weight change |
|---|---|
| `--homeostasis=off` | 5.27e-05 |
| `--homeostasis` | 5.38e-05 (noise) |
| `--homeostasis --homeostasis-eta=1.0` | 5.70e-04 |

The flag read as "homeostasis is on" while nothing was scaled. Fixed on both the
general and Phase-C paths: when `--homeostasis` is given without
`--homeostasis-eta`, eta defaults to 0.01 and the substitution is printed. After
the fix `--homeostasis` alone moves avg weight change 5.75e-05 -> 5.42e-04.

This is the same defect class as the `--enable-learning` zero-rates bug fixed
2026-08-22: a documented flag that initialises a subsystem with parameters that
disable it.

## 7. Unrelated: the 2M-neuron benchmark does not measure 2M neurons

While tracing where "2,000,000 neurons, 99 active synapses" came from:

- `benchmark_2m_results*.json`: of six runs, three exited 0, two exited 2, one
  exited 1. `PeakMemoryMB` is 0 or absent throughout.
- In the exit-0 runs, `NeuronSteps: 2000000000` is exactly
  `Configuration.Neurons x Steps` — a product of the declared config, not a
  measurement. `NeuronStepsPerSecond` is that product over wall-clock.
  `MemoryUsageMB: null`.
- `2m_neural_assembly_report.json` reports `total_neurons: 2000000` alongside
  `active_synapses: 99`, `analyzed_connections: 198`, and a largest neural
  assembly of 7.

Nothing in `src/` creates 2,000,000 neurons or calls
`setProceduralConnectivity()`, and `procedural_connectivity_enabled_` defaults
to false. The throughput figure is arithmetic over a config value; it does not
establish that the network was built or connected at that scale.

(Worth knowing: `connectRegions` has a procedural branch that returns a
*count* of virtual synapses without instantiating any. It is currently
unreachable from `main.cpp`, but it is the branch a future scale claim would
most easily be built on by accident.)

## Changes made

- `main.cpp`: `--demo-neurons=`, `--demo-density=` (default brain size/density)
- `main.cpp`: report `Total Neurons` / `Active Neurons (%)` on both stats paths
- `main.cpp`: default `homeostasis_eta` to 0.01 when `--homeostasis` is given
  bare, on both the general and Phase-C paths, with a printed notice
- `main.cpp`: stop the second argument parser warning "unrecognized option" for
  flags the primary parser already consumed (`--ablate*`, `--sequential`, and
  the two new ones)

## Reproducibility note

`--sequential` is required for exact reproducibility. Without it, four identical
runs gave 66996 / 67004 / 66996 / 66996 total updates — a spread of 8 on 67,000
(0.012%). With it, all four gave 67004. The spread is far below the effects
reported here but is not zero; every measurement above used `--sequential`.
