# Normalising the decision inputs: the input becomes decisive

Date: 2026-08-23
Follows: `sensory_drive_2026-08-23.md`, which found the sensory pathway causal to
prefrontal cortex but unable to change which option the brain selected.

## Result

With per-channel normalisation, the sensory drive changes **~55%** of prefrontal
decisions against **~5%** run-to-run noise — a clean separation with no overlap.
Before normalisation the choice sequence was identical with and without input.

Getting a trustworthy measurement required a second fix, and the first attempt at
this result was wrong. Both are recorded below.

## 1. The change

The prefrontal options were raw region activations, and the four source regions
sit at very different resting levels — Thalamus ~0.56-0.70 against Hippocampus
~0.19 and Amygdala ~0.19. A sensory perturbation of ~0.04 cannot reorder a set
separated by ~0.35, so the decision was dominated by *which region* a channel is
rather than what it currently carries.

`ChannelNormaliser` keeps, per channel, an exponential moving average of the
baseline and a second of the typical absolute deviation, giving a running z-like
score squashed back into [0,1] with `tanh`:

```cpp
const float dev = raw - st.baseline;
st.baseline += kAlpha * dev;                        // kAlpha = 0.05
st.scale    += kAlpha * (std::fabs(dev) - st.scale);
return 0.5f + 0.5f * std::tanh(dev / std::max(st.scale, kMinScale));
```

The value range downstream is unchanged, so `makeDecision` keeps seeing numbers
shaped like activations; only their meaning changes, from "how active" to "how
unusual for this channel". The first sample of a channel returns a neutral 0.5 —
with no history there is no deviation to report, and inventing one would be a
fabricated signal.

This is also the more defensible model: cortex responds to change against
expectation, not to absolute firing level.

The decision log now prints both, so the two are directly comparable:

```
[Decision] PFC option 1 of 4 conf=0.954 norm=[0.937 0.899 0.935 0.899] raw=[0.958 0.308 0.430 0.652]
```

Immediate effect: the decision went from **6 changes** over a run to **~60-75**.

## 2. The first measurement was wrong

One run per condition showed the choice sequences differing at 2 of 78 positions,
which looked like confirmation. It was not. Running three per condition:

```
WITHIN off:  51/75,  0/76, 51/75
WITHIN on :   1/72, 47/71, 47/71
BETWEEN   :  0/72 ... 51/75
```

Two runs of the **same** condition differed by up to 51 of 75 positions. The
2/78 "effect" sat entirely inside that. A single run per arm could not have
distinguished signal from noise, and nearly did not.

## 3. Why: the autonomous loop races the main loop

The decision blocks live only in `runAutonomousLoop`, which `main` launches on
its own thread while the main step loop keeps running:

```cpp
autonomous_thread = std::thread([&brain, steps, &autonomous_running]() {
    brain.runAutonomousLoop(static_cast<std::size_t>(steps), 10.0f);
});
```

How many autonomous cycles interleave with how many main steps is left to the
scheduler. Measured at `--steps=60`, three identical invocations:

| | run 1 | run 2 | run 3 |
|---|---|---|---|
| Thalamus mean act, **no** autonomous mode | 0.6955 | 0.6955 | 0.6955 |
| Thalamus mean act, threaded autonomous | 0.8836 | 0.8835 | 0.8809 |

Without autonomous mode the brain is bit-identical across runs. With the thread
it is not. Normalisation did not create this — it amplified it into visibility,
which is what a z-score against a small running scale does to jitter.

## 4. The second fix: `--autonomous-sync`

Runs the autonomous loop inline instead of on a thread. Since the loop drives its
own `processStep()`, sensory input injected from the main step loop would never
coincide with the decisions taken inside it, so `HypergraphBrain::setPreCycleHook`
was added — called at the top of each iteration, and used to inject the visual
grid.

Within-condition variation drops from ~68% to ~5%:

| | within off | within on | between |
|---|---|---|---|
| threaded | up to 51/75 (68%) | up to 47/71 (66%) | 0/72 - 51/75 |
| **synchronous** | **5/63, 6/63, 1/63** | **3/58, 0/54, 3/54** | **30/54 - 37/62** |

Max within-condition: 9.5%. Min between-condition: 55.6%. No overlap, roughly a
10:1 ratio.

**The sensory input now changes what the brain decides.**

The threaded path is unchanged and remains the default; `--autonomous-sync` is
opt-in and is what any measured claim about decisions should use.

## 5. What this establishes, and what it does not

**Does:** normalising the decision inputs makes an external sensory signal
decisive, and the effect is an order of magnitude above the residual noise. The
hypothesis recorded in `sensory_drive_2026-08-23.md` — that baseline dominance
was what blocked it — is supported.

**Does not:** that the brain decides *well*. Nothing scores the outcome, so a
changed decision is not yet a better one. The motor command is planned and
executed, but nothing observes the result, so there is no signal that one choice
beat another and the decision can be made but not learned.

Residual ~5% within-condition variation also remains, from the same source as the
0.004% activity spread noted earlier — one of 172 `::now()` reads in `src/core`
that has not been located. It is small enough not to threaten this result, and it
should be closed before finer effects are chased.

## Verification

- Default path unchanged: 148,604 updates / 102 synapses.
- Non-autonomous anatomical brain still exact: Thalamus 0.6955 x 3.
- Threaded autonomous mode still functions (decisions still logged).
- Full build: 106/106, 0 errors.
- Test sweep: 33 pass, 3 fail — the pre-existing baseline.
