#!/usr/bin/env python3
"""Run NeuroForge arms across seeds and analyse them with falsify.

WHY THIS EXISTS
---------------

Every experiment in the 2026-08-22..24 session was analysed by hand: run an arm,
grep the output, average it, eyeball the difference. Four claims were retracted,
and all four traced to the same omission -- comparing conditions without first
measuring within-condition variance. The clearest case reported "the learned
policy improves 4/4 runs" when all four runs of each arm had produced *identical*
first-half values, i.e. one deterministic trajectory repeated rather than four
independent confirmations.

`falsify` already implements the checks that catch this. This module is the
adapter: it runs NeuroForge invocations, parses metrics out of stdout, and hands
seed-keyed dicts to `falsify.paired`.

A METHODOLOGICAL CORRECTION IT FORCES
-------------------------------------

The session's runs used ONE seed (401) repeated N times, relying on residual
nondeterminism to produce variation. `falsify.paired` requires arms to share
seeds and warns below MIN_SEEDS=5, so using it forces the correct design: the
same set of distinct seeds through every arm, paired per seed. Repeats at a fixed
seed measure the noise floor of the implementation; different seeds measure
whether an effect survives different initial conditions. They are not
interchangeable, and the session conflated them.

USAGE
-----

    from nf_experiment import Arm, run_arm, compare

    base = Arm("argmax", [])
    learn = Arm("learned", ["--learn-policy"])
    common = CLOSED_LOOP + ["--steps=1500"]

    b = run_arm(base,  seeds=[401,402,403,404,405], common=common)
    t = run_arm(learn, seeds=[401,402,403,404,405], common=common)
    print(compare(b, t, "mean_distance", higher_is_better=False).summary())

Or from the command line:

    python tools/nf_experiment.py --demo
"""

from __future__ import annotations

import os
import re
import statistics as st
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Sequence

# --- locate falsify -----------------------------------------------------------

_FALSIFY_SRC = Path(
    os.environ.get(
        "FALSIFY_SRC",
        Path(__file__).resolve().parents[2] / "Architecture_Substrate_Research" / "falsify" / "src",
    )
)
if _FALSIFY_SRC.is_dir() and str(_FALSIFY_SRC) not in sys.path:
    sys.path.insert(0, str(_FALSIFY_SRC))

try:
    from falsify import MIN_SEEDS, floor_p, manipulation_check, paired, seeds_needed
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        f"cannot import falsify from {_FALSIFY_SRC}\n"
        "set FALSIFY_SRC to the directory containing the `falsify` package"
    ) from exc


# --- running NeuroForge -------------------------------------------------------

REPO = Path(__file__).resolve().parents[1]
BINARY = REPO / "build" / "neuroforge.exe"

# The mingw64 runtime the binary is linked against. Without this on PATH the
# process exits 0xC0000139 (entrypoint not found) with no output, which reads
# exactly like a crash.
MINGW_BIN = r"C:\msys64\mingw64\bin"

#: Flags common to every closed-loop experiment. Kept here rather than repeated
#: at call sites so an arm's flag list contains ONLY what distinguishes it --
#: which is what makes an arm comparison honest.
CLOSED_LOOP: List[str] = [
    "--enable-learning",
    "--sequential",
    "--anatomical-regions",
    "--autonomous-mode",
    "--autonomous-sync",
    "--enable-pfc",
    "--enable-motor-cortex",
    "--closed-loop",
    "--auto-eligibility=on",
]


@dataclass
class Arm:
    """One experimental condition: a name and the flags that define it."""

    name: str
    flags: List[str] = field(default_factory=list)


@dataclass
class RunResult:
    seed: int
    metrics: Dict[str, float]
    ok: bool
    stdout: str = ""


_PATTERNS = {
    "total_updates": r"Total Updates:\s*(\d+)",
    "active_synapses": r"Active Synapses:\s*(\d+)",
    "total_neurons": r"Total Neurons:\s*(\d+)",
    "active_neurons": r"Active Neurons:\s*(\d+)",
    "phase4_updates": r"Phase-4 Updates:\s*(\d+)",
    "potentiated": r"Potentiated Synapses:\s*(\d+)",
    "depressed": r"Depressed Synapses:\s*(\d+)",
    "avg_weight_change": r"Avg Weight Change:\s*([0-9.eE+-]+)",
}


def parse_metrics(out: str) -> Dict[str, float]:
    """Pull scalar metrics and the closed-loop trajectory out of a run's stdout."""
    m: Dict[str, float] = {}
    for key, pat in _PATTERNS.items():
        hit = re.search(pat, out)
        if hit:
            m[key] = float(hit.group(1))

    # Closed-loop task performance: mean distance to the light over the run.
    dists = [int(x) for x in re.findall(r"\[World\].*?dist=(\d+)", out)]
    if dists:
        m["mean_distance"] = st.mean(dists)
        m["final_quarter_distance"] = st.mean(dists[3 * (len(dists) // 4):])
        m["n_world_samples"] = float(len(dists))

    # Decision behaviour, for manipulation checks on the policy.
    choices = re.findall(r"\[Decision\] PFC option (\d+) of", out)
    if choices:
        m["n_decision_changes"] = float(len(choices))
        m["n_distinct_actions"] = float(len(set(choices)))

    if "potentiated" in m and m.get("depressed"):
        m["pot_dep_ratio"] = m["potentiated"] / m["depressed"]
    return m


def run_once(arm: Arm, seed: int, common: Sequence[str], timeout: int = 900) -> RunResult:
    if not BINARY.exists():
        raise SystemExit(f"binary not found: {BINARY}  (build it first)")
    env = dict(os.environ)
    if os.path.isdir(MINGW_BIN):
        env["PATH"] = MINGW_BIN + os.pathsep + env.get("PATH", "")
    cmd = [str(BINARY), f"--phase-c-seed={seed}", *common, *arm.flags]
    try:
        proc = subprocess.run(
            cmd, capture_output=True, text=True, timeout=timeout, env=env,
            errors="replace",
        )
    except subprocess.TimeoutExpired:
        return RunResult(seed, {}, ok=False, stdout="TIMEOUT")
    out = (proc.stdout or "") + (proc.stderr or "")
    return RunResult(seed, parse_metrics(out), ok=(proc.returncode == 0), stdout=out)


def run_arm(
    arm: Arm,
    seeds: Sequence[int],
    common: Sequence[str],
    *,
    verbose: bool = True,
) -> Dict[int, Dict[str, float]]:
    """Run one arm across seeds. Returns {seed: metrics}.

    Failed runs are dropped with a warning rather than silently, because a
    missing seed changes which pairs exist and `paired` reports that.
    """
    out: Dict[int, Dict[str, float]] = {}
    for s in seeds:
        r = run_once(arm, s, common)
        if r.ok and r.metrics:
            out[s] = r.metrics
            if verbose:
                d = r.metrics.get("mean_distance")
                extra = f" mean_distance={d:.3f}" if d is not None else ""
                print(f"  [{arm.name}] seed {s}: ok{extra}", flush=True)
        else:
            print(f"  [{arm.name}] seed {s}: FAILED ({r.stdout[:60]!r})", flush=True)
    return out


# --- analysis -----------------------------------------------------------------


def series(runs: Dict[int, Dict[str, float]], metric: str) -> Dict[int, float]:
    """{seed: value} for one metric, skipping seeds where it was not reported."""
    return {s: m[metric] for s, m in runs.items() if metric in m}


def compare(
    baseline: Dict[int, Dict[str, float]],
    treatment: Dict[int, Dict[str, float]],
    metric: str,
    *,
    higher_is_better: bool = True,
):
    """Paired comparison of one metric. Thin wrapper so call sites read clearly."""
    return paired(
        series(baseline, metric),
        series(treatment, metric),
        higher_is_better=higher_is_better,
    )


def within_condition_spread(runs: Dict[int, Dict[str, float]], metric: str) -> float:
    """Max-minus-min across seeds within ONE arm.

    The number every retracted claim in this project needed and did not have. An
    effect smaller than this is not an effect, whatever its mean says.
    """
    vals = list(series(runs, metric).values())
    return (max(vals) - min(vals)) if len(vals) > 1 else 0.0


def report(
    name: str,
    baseline: Dict[int, Dict[str, float]],
    treatment: Dict[int, Dict[str, float]],
    metric: str,
    *,
    higher_is_better: bool = True,
) -> None:
    """Print a comparison with the context that makes it interpretable."""
    print(f"\n=== {name}: {metric} ===")
    b_spread = within_condition_spread(baseline, metric)
    t_spread = within_condition_spread(treatment, metric)
    print(f"within-condition spread: baseline {b_spread:.4f}  treatment {t_spread:.4f}")

    res = compare(baseline, treatment, metric, higher_is_better=higher_is_better)
    print(res.summary())

    worst = max(b_spread, t_spread)
    if worst and abs(res.mean_delta) <= worst:
        print(f"  ! effect {abs(res.mean_delta):.4f} does NOT exceed the "
              f"within-condition spread {worst:.4f} — treat as null")
    elif worst:
        print(f"  effect {abs(res.mean_delta):.4f} exceeds within-condition "
              f"spread {worst:.4f}")

    if res.sd and abs(res.mean_delta) > 0:
        need = seeds_needed(res.mean_delta, res.sd)
        if need > res.n:
            print(f"  seeds for a 2:1 effect/sem ratio: {need} (have {res.n})")


def check_metric_moves(
    off: Dict[int, Dict[str, float]],
    on: Dict[int, Dict[str, float]],
    metric: str,
    *,
    min_ratio: float = 2.0,
):
    """Manipulation check: did the mechanism change the system's internals at all?

    A behavioural null is only informative if the thing under test demonstrably
    did something. Six substrate-learning nulls in this project rest on exactly
    this check passing.
    """
    a = list(series(off, metric).values())
    b = list(series(on, metric).values())
    if not a or not b:
        return None
    return manipulation_check(st.mean(a), st.mean(b), min_ratio=min_ratio)


def main(argv: Optional[List[str]] = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    if "--demo" not in argv:
        print(__doc__)
        print(f"binary:  {BINARY}  ({'found' if BINARY.exists() else 'MISSING'})")
        print(f"falsify: {_FALSIFY_SRC}")
        print(f"MIN_SEEDS={MIN_SEEDS}  floor_p(5)={floor_p(5):.4f}  "
              f"floor_p(8)={floor_p(8):.5f}")
        return 0

    seeds = [401, 402, 403, 404, 405]
    common = [*CLOSED_LOOP, "--steps=1500", "--motor-selection", "--action-credit"]
    print(f"seeds={seeds}  floor_p={floor_p(len(seeds)):.4f}")

    base = run_arm(Arm("argmax", []), seeds, common)
    learn = run_arm(Arm("learned-policy", ["--learn-policy"]), seeds, common)

    report("policy learner", base, learn, "mean_distance", higher_is_better=False)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
