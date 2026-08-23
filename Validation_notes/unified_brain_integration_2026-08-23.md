# Does the unified brain actually run?

Date: 2026-08-23
Follows: `structural_plasticity_and_anatomy_2026-08-23.md`

Two tasks: fix the wall-clock ageing that made the anatomical brain
irreproducible, and find out whether the four region-integration blocks in
`runAutonomousLoop` ever execute.

## Headline

They execute now. They did not before, and there was no way to tell.

```
[Integration] MotorCortex:       bound to its region class, integration ACTIVE
[Integration] PrefrontalCortex:  bound to its region class, integration ACTIVE
[Integration] SelfNode:          bound to its region class, integration ACTIVE
```

That means `pfc->makeDecision(...)`, `pfc->storeInWorkingMemory(...)`,
`mc->planMovement(...)`, `mc->executeMotorCommands()` and
`selfNode->initiateReflection(...)` are running for the first time.

## 1. Why the integration was dead

`HypergraphBrain::runAutonomousLoop` reaches for four regions by name and
downcasts each before use:

```cpp
auto prefrontal_cortex = getRegion("PrefrontalCortex");
if (prefrontal_cortex) {
    auto pfc = std::dynamic_pointer_cast<Regions::PrefrontalCortex>(prefrontal_cortex);
    if (pfc) {
        auto decision = pfc->makeDecision(options, values);
        pfc->storeInWorkingMemory(planning_context);
    }
}
```

Two independent reasons it never ran, and neither produced any output:

1. **`getRegion("PrefrontalCortex")` returned null.** The default brain builds
   `DemoCortex` and `DemoSubcortex`; no region carries these names, so the outer
   `if` failed and the block was skipped.
2. **Even with the name present, the cast returned nullptr**, because
   `RegionFactory` returned a base `Region` for every name until the anatomical
   dispatch added in d7326f1.

So the integration reported `ENABLED` while doing nothing. There was no error, no
log line, and no failing check — `if (pfc)` simply fell through forever.

A third constraint, worth stating because it is not obvious: these blocks live
**only** in `runAutonomousLoop`. They are not on the normal `--steps=N` path at
all, so `--enable-pfc` alone can never do anything without `--autonomous-mode`.
That is why an earlier attempt to detect a behavioural difference from
`--enable-pfc` found none.

## 2. Making the binding observable

Added `reportIntegrationBinding(region_name, bound)`, called once per region at
each of the four sites. It prints ACTIVE when the downcast succeeds and an
explicit INERT warning when the region exists but is a plain `Region`.

This is the point of the change as much as the fix is. The failure mode here was
not a wrong result — it was a subsystem that reported enabled, produced no error,
and did nothing. That is the same shape as `--enable-learning` with zero rates,
`--homeostasis` with eta=0, and `enable_structural_plasticity` with zero batches.
A silent `if (ptr)` is that pattern in pointer form.

Evidence the check discriminates:

| invocation | output |
|---|---|
| `--autonomous-mode --enable-pfc ...` | *no `[Integration]` lines at all* — region absent, cast never reached |
| `--anatomical-regions --autonomous-mode --enable-pfc ...` | three ACTIVE lines |

`SelfNode` was added to the anatomical set for exactly this reason: the
reflection integration looks it up by name, so without it that block finds no
region. The brain is now twelve regions.

```
Total Neurons:    768
Active Synapses:  2,876
Active Neurons:   642 (83.6%)
```

## 3. The wall-clock fix

`Insula` aged interoceptive signals against `system_clock`:

```cpp
auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - signal.timestamp);
if (duration.count() < 10) { ... }   // valid for 10 seconds
return duration.count() > 30;         // erased after 30 seconds
```

A slower run crosses those thresholds at a different step, keeps or drops a
different set of signals, and produces different downstream activity.

Added `Region::simTimeSeconds()`, accumulated from the `delta_time` passed to
`process()`, and converted the three Insula sites to it. `InteroceptiveSignal`
keeps its `timestamp` for logging and gains `sim_time_seconds` for decisions.
With `step_ms = 10`, one step is 0.01 simulated seconds, so the 10s and 30s
thresholds are now crossed at the same step on every run and on every machine.

Result: the anatomical brain went from **three** distinct update totals across
eight runs to **two**. One confirmed source removed.

## 4. Determinism: improved, still not exact

| configuration | 8 runs |
|---|---|
| demo path | 148,604 x 3 — exact |
| anatomical, structure (`Active Synapses`) | 2,876 x 4 — **exact** |
| anatomical, activity (`Total Updates`) | 1,013,988 / 1,014,016 / 1,014,030 |

The structure is exactly reproducible; the activity is not. The spread is
1,013,988 to 1,014,030 — 42 counts on ~1.01M, or **0.004%**.

What is established:

- The demo path is exact, so the source is in the anatomical region code, not the
  core learning loop.
- Insula was one source and is fixed.
- It is **not** the remaining `system_clock` calls in `LimbicRegions.cpp`.
  `SelfNode`'s three sites (lines 392, 403, 539) only *write* `last_updated`;
  nothing ever compares it. `CingulateCortex`'s `detection_time` is likewise
  write-only. The one surviving `duration_cast` is in `DefaultModeNetwork`, which
  is not in the region set.
- The two-value spread predates adding `SelfNode`, so `SelfNode` widened it
  rather than introducing it.
- `std::random_device` is clean — it appears only inside `DeterministicRng`
  itself, by design.

What is not established: which of the **172** `::now()` reads across `src/core`
feeds a decision. Auditing all of them was out of scope for this pass.

**Guidance unchanged:** the anatomical brain is sound for structural work, where
it is exact, and not yet for paired activity comparisons. The demo path remains
exact and is the right substrate for those.

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Full build: 106/106 targets, 0 errors.
- Test sweep: 33 pass, 3 fail — the pre-existing baseline (`test_memorydb`,
  `test_phase2_memory`, `test_substrate_language_integration`).

## Open

1. The residual 0.004% activity spread, above.
2. `DefaultModeNetwork::getCurrentThoughts` still ages thoughts against the wall
   clock (line 709). Harmless today because the class is not constructed; it will
   bite the moment it is.
3. The integration blocks call fixed placeholder data — `options = {0.2, 0.5,
   0.8, 0.3}`, `movement_vector = {0.1, 0.0, 0.2}`. They now genuinely execute,
   but they are not yet driven by anything the brain perceived. Making them
   ACTIVE was the prerequisite; wiring real inputs into them is the next step.
