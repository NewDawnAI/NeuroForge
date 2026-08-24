# Session record, 2026-08-22 → 2026-08-24

Index and synthesis for thirteen validation notes. Read this first; each section
points at the note holding the measurements.

Branch `fix/governance-and-learning-defaults`, commits `135f4bd` → `c8034f6`.

---

## 1. What changed, in one table

| | before | after |
|---|---|---|
| reproducible from a seed | no — 44,660 / 49,064 / 47,718 / 47,252 updates on identical invocations | yes, with `--sequential` |
| synapse count | 102, at every step count, forever | 102 → 1,048,576 by configuration; grows and prunes at runtime |
| n=8192 | >900 s, killed | 73 s |
| region classes constructed | 0 of 14 | 12, opt-in |
| neuron activity reported | not at all | per-region and aggregate |
| region integrations (PFC/Motor/SelfNode) | silently inert | ACTIVE, and observable either way |
| Phase-4 reward updates | 0 | 237,409 |
| behaviour under reward | unchanged | phototaxis solved, 2.139 → 0.967 |

Default path unchanged throughout: **148,604 total updates, 102 active
synapses**. Every feature above is opt-in. Full build 106/106; test sweep 33 pass
/ 3 fail, matching the pre-existing baseline.

---

## 2. The dominant defect: enabled-but-inert

Six instances of one shape — a flag or setting that reports a subsystem on while
its parameters, or its plumbing, leave it doing nothing. No error, no warning, no
failing check.

| # | subsystem | why it did nothing | note |
|---|---|---|---|
| 1 | `--enable-learning` | `hebbian_rate` / `stdp_rate` default 0.0 → 0 updates | committed 2026-08-22 |
| 2 | `--homeostasis` | `homeostasis_eta` defaults 0.0 → `applyHomeostasis` returns on line 1 | `homeostasis_and_connectivity` |
| 3 | `enable_structural_plasticity` | both batch sizes default 0 → growth skipped; **and no CLI flag existed at all** | `connectome_plasticity_audit` |
| 4 | region integrations | `dynamic_pointer_cast` returned nullptr forever; `if (pfc)` fell through | `unified_brain_integration` |
| 5 | `--auto-eligibility` | defaults false → `dw = κ·R·0` → Phase-4 reported 0 updates | `why_it_does_not_learn` |
| 6 | 14 region classes | `RegionFactory` always returned base `Region`; `NF_ForceLink_*` are empty bodies | `structural_plasticity_and_anatomy` |

**The generalisation worth keeping:** in this codebase, "the flag is set" and "the
subsystem runs" are independent facts. Every claim about a subsystem needs a
manipulation check — does a metric move at all under an extreme parameter value?
Several hours went into reasoning about subsystems that never executed.

Instance 4 is the subtle one: a silent `if (ptr)` is this pattern in pointer
form. The fix that mattered was not the cast but
`reportIntegrationBinding()`, which makes the binding observable.

---

## 3. Performance: three algorithmic defects, not tuning

See `scaling_to_n8192` and `why_it_does_not_learn`.

1. **A global statistic recomputed per weight update.**
   `LearningSystem::updateStatistics` walked every neuron of every region to
   recount `active_synapses`, and is called once per weight update from 7 sites.
   O(synapses × neurons) per step — ~3.7 billion neuron visits per step at
   n=2048, under a mutex. Also unnecessary: the count is structural and a weight
   update cannot change it. Moved to the read path. 38× faster per step.

2. **"Pre-reserve to avoid reallocation" forced one realloc per insert.**
   `reserve(size + 1)` called once per synapse, on a vector holding every synapse
   between a region pair (131,072 entries at n=1024). Bulk insertion became
   O(n²) refcounted `shared_ptr` copies. The comment described the opposite of
   the behaviour. Construction 74 s → 2 s at n=2048.

3. **A deadlock misdiagnosed as slowness.** `--auto-eligibility` appeared to make
   the brain 100× slower and survived four optimisations aimed at that theory.
   `Region::getNeuron` took `region_mutex_`; spike callbacks fire from code
   already holding it; `std::mutex` is not recursive. It deadlocked on the first
   spike.

**Method note on (3):** two measurements found it after four rounds of optimising
did not. `--steps=1` timed out exactly like `--steps=20` — cost that does not
scale with work is not a performance problem — and a probe printed 9 "about to
getNeuron" against 8 "returned". Checking `--steps=1` should have come first.

---

## 4. Design defects found while verifying

- **Synapses were born condemned.** `growSynapses` births at weight 0.05 and
  `structural_prune_threshold` also defaults to 0.05, so every new synapse was
  created at exactly the weight that makes it eligible for deletion. Newborns now
  start at twice the threshold.
- **Behaviour gated on wall-clock time.** `Insula` aged interoceptive signals
  against `system_clock` (valid <10 s, erased >30 s), so a slower run kept a
  different set. Added `Region::simTimeSeconds()`, accumulated from `delta_time`.
- **`processVisualInput` discarded 75% of the visual field.** It wrote
  `visual_input[i]` into neuron `i` of *every* layer; `initializeLayers` splits 64
  neurons into 4 layers of 16, so only the first 16 values were read, replicated
  four times.
- **A test read a launch failure as a result.** `std::system()` returns -1 when
  the command processor cannot start. Two tests read `rc != 0` and reported false
  failures; **three** read `rc == 0` and reported false passes — they would pass
  with the binary never launched.

---

## 5. Retractions

Recorded because the pattern matters more than any single number.

| claim | status | how it broke |
|---|---|---|
| "the anatomical brain is deterministic, 846,766 × 4 runs" | **wrong** | 4 runs landed on one value by chance; 8 runs give 3 |
| "`test_learning` prints PASSED and exits 1" | **wrong** | two different runs conflated; `main()` was always correct |
| "the learned policy improves 4/4 within runs" | **wrong** | all four `off` first-halves were exactly 1.08 and all four `on` exactly 2.17 — one deterministic trajectory repeated, not four confirmations |
| "eligibility accumulation is too slow" | **wrong** | it was a deadlock; four optimisations were spent on the wrong theory |

**The recurring error is reading repetition as independence.** Identical values
across supposedly independent runs are evidence of a *shared deterministic path*,
not of a robust effect. Twice this session that inverted a conclusion.

**The rule that would have caught all four:** measure within-condition variance
before comparing conditions. It is now the first step in every comparison here,
and it is why the final result is trustworthy: within-condition spread 0.08 and
0.73 against a between-condition effect of 1.172, with no overlap.

---

## 6. Confirmed results

Each survived a within-condition variance check.

**Structural plasticity is causal.** 600 steps: off 102, `grow=8` 118, `grow=32`
162, `+interval=25` 307, and the same with `prune=0.5` back to 102 — aggressive
pruning cancels aggressive growth exactly.

**The anatomical regions behave differently from generic ones.** The demo brain
saturates at 100% active; the 12-region brain sits at 83.6%. First behavioural
evidence the region implementations do anything.

**Homeostasis is a well-powered null** on both synapse count and activity: 7,600×
change in average weight change, bit-identical counts.

**The sensory pathway is causal to prefrontal cortex.** Zero variance within
condition; Thalamus 0.6955 → 0.7367, PFC 0.1780 → 0.1562 under drive.
VisualCortex's active count halves 64 → 32, exactly what an 8×8 checkerboard
should give.

**The policy learns.** 1,500 steps, 3 runs per arm: fixed argmax 2.139, learned
0.967, effect +1.172 against max within-spread 0.733, no overlap. Learning curve
in every run from uniform weights.

---

## 6b. Substrate credit assignment: three fixes, still a null

Pursued after the closed-loop result, because the policy learner sat *beside* the
substrate rather than in it.

| attempt | change | result |
|---|---|---|
| 1 | three-factor eligibility (`rate × pre × post`) + action gate | +0.244, inside noise |
| 2 | eligibility decay (traces never decayed — they saturated and stayed) | still inside noise |
| 3 | selection reads motor action channels, closing the credit→choice path | +0.011 |
| 4 | reward baseline | **+0.006** — diagnosis was wrong; see below |

Each fixed something real; three defects had to be removed before the fourth
became visible.

**The fourth was diagnosed as a missing reward baseline. That diagnosis was
wrong.** A baseline was implemented and tested: +0.006 against a within-spread of
0.367, a null, predicted in advance from a mechanism check.

`(1[a==chosen] − p[a])·x` is **∇log π for a softmax** — the score function, not
an advantage or baseline. The policy layer runs `lr·R·∇log π`, which is REINFORCE
and contains no baseline. The substrate runs `κ·R·eligibility`, multiplying
reward by a *coincidence* trace: reward-modulated Hebbian, which does not ascend
the gradient of expected reward. The gap is the **learning rule**, not a missing
term in it.

(An EMA baseline is also a no-op on a mean-zero reward, independently.)

Superseded reasoning follows:

~~The fourth is a missing reward baseline.~~ Reward reaches the weights — ~2M
Phase-4 updates per run — but potentiation and depression are nearly balanced
(5.5–5.9M against 4.1–4.5M), so the weights random-walk. `dw = kappa·R·elig` has
no baseline and reward has a mean near zero, so updates cancel. The policy layer
succeeds on the identical signal because its update carries an advantage term.

Credit assignment says *which* synapses; a baseline says *how much better than
expected*. Fixing the first without the second gives correctly-targeted noise.

## 7. Open, in priority order

1. **Phase-4 has no reward baseline.** Credit assignment is now fixed (see 6b);
   the remaining gap is `dw = kappa·(R − R̄)·elig`, a critic. Biologically this is
   the difference between reward and reward *prediction error*.
1. ~~Phase-4 has no credit assignment.~~ **Done** — `onNeuronSpike` bumps every synapse of
   every spiking neuron by a flat 0.1, and 83% of neurons are active, so
   eligibility records "was active" rather than "was responsible". `dw = κ·R·elig`
   is then approximately one scalar applied to everything recently active, which
   shifts all activations together and preserves their ordering. **This is the
   real target:** the substrate does not learn; a policy layer beside it does.
2. **Residual nondeterminism.** ~0.004% on activity in the anatomical brain, and
   ~5% on decision sequences. One of **172** `::now()` reads in `src/core` feeds a
   decision; not located. Structure is exact, so this blocks activity-level
   paired comparisons only.
3. **Memory at scale.** ~657 bytes/synapse → ~36 GB at fly scale. Needs a flat
   arena with integer indices, not `shared_ptr` graphs. Raising
   `max_per_source_cap` from 64 to a fly-like ~390 multiplies memory by ~6, so it
   is the second problem, not the first.
4. **`Region::getNeuron` is still a linear scan** called twice per synapse in
   construction. Not currently the bottleneck.
5. **Neurogenesis untested.** `--structural-spawn-batch` is wired and defaults to
   0; no measurement exercises it.
6. **Three pre-existing test failures** (`test_memorydb`, `test_phase2_memory`,
   `test_substrate_language_integration`), verified against HEAD as not
   regressions. The third is failing correctly — it was converted from a false
   pass on 2026-08-22 and never fixed.
7. **`DefaultModeNetwork::getCurrentThoughts`** still ages thoughts on the wall
   clock. Harmless while the class is not constructed; it will bite when it is.
8. **The CLI checks in `test_learning` now skip** rather than run, because
   spawning a full brain per flag check needs ~650 MB. Honest reporting is not
   coverage.

---

## 8. Notes index

| note | subject |
|---|---|
| `governance_engine_changes_2026-08-22` | privilege escalation closed; check ordering by jurisdiction |
| `scaling_to_n8192_2026-08-23` | two O(N²) defects; memory per synapse; fly-scale projection |
| `test_learning_cli_spawn_2026-08-23` | launch failure read as a test result |
| `homeostasis_and_connectivity_2026-08-23` | `--homeostasis` no-op; what "active synapses" measures |
| `connectome_plasticity_audit_2026-08-23` | the connectome is frozen; regions never constructed; HDC assessment |
| `structural_plasticity_and_anatomy_2026-08-23` | making growth reachable; anatomical dispatch |
| `unified_brain_integration_2026-08-23` | the four dead integrations; wall-clock ageing |
| `sensory_drive_2026-08-23` | external input propagates, does not yet decide |
| `normalised_decision_inputs_2026-08-23` | per-channel normalisation; `--autonomous-sync` |
| `prereg_subsystem_knockout_2026-08-23` | pre-registration for the knockout study |
| `closed_loop_2026-08-24` | phototaxis task; reward wiring |
| `why_it_does_not_learn_2026-08-24` | the deadlock; four reasons learning was impossible |
| `it_learns_2026-08-24` | learnable policy; three observation defects; the result |

Flag reference: `docs/FLAGS_ADDED_2026-08.md`.

---

## 9. Method, as practised here

What repeatedly worked:

- **Measure within-condition variance before comparing conditions.** Four
  retractions trace to skipping this.
- **Distinguish "code exists" from "code executes".** A grep proves neither.
- **Include a manipulation check.** If an extreme parameter value moves no
  metric, the subsystem is not running and any null is uninformative.
- **Check whether cost scales with work** before optimising. A flat cost is a
  hang, not a slow path.
- **Prefer a control that should cancel the effect.** Pruning cancelling growth
  established both were live more convincingly than either alone.
- **Make silent failure observable.** `reportIntegrationBinding`, the `[Sensory]
  INERT` warning and the change-triggered `[Decision]` log each turned a
  no-op into something a run reports about itself.
