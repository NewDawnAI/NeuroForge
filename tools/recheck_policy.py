#!/usr/bin/env python3
"""Re-run the session's headline result under proper seeding.

The original measurement -- fixed argmax 2.139 vs learned policy 0.967, "no
overlap" -- used ONE seed (401) repeated three times at 1500 steps, with the
control selecting over input regions. The harness validation changed three things
at once (900 steps, 8 distinct seeds, a control including --motor-selection
--action-credit) and reported a null, which is uninterpretable as evidence about
the original claim.

This reproduces the ORIGINAL configuration exactly and changes only the seeding.
If the effect survives, the original result stands and the validation differed
for configuration reasons. If it does not, the headline result was an artifact of
measuring one seed repeatedly.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from nf_experiment import CLOSED_LOOP, Arm, floor_p, report, run_arm, within_condition_spread

SEEDS = [401, 402, 403, 404, 405, 406, 407, 408]
# Original configuration: 1500 steps, NO --motor-selection / --action-credit.
COMMON = [*CLOSED_LOOP, "--steps=1500"]

print(f"ORIGINAL config, proper seeding: n={len(SEEDS)} floor_p={floor_p(len(SEEDS)):.5f}")
print("control = argmax over input regions (as originally measured)\n")
ctl = run_arm(Arm("argmax", []), SEEDS, COMMON)
lrn = run_arm(Arm("learned", ["--learn-policy"]), SEEDS, COMMON)
report("headline result, re-seeded", ctl, lrn, "mean_distance", higher_is_better=False)
print("\noriginal hand-measured: control 2.139, learned 0.967, effect 1.172")
import statistics as st
for n,a in (("control",ctl),("learned",lrn)):
    v=[m["mean_distance"] for m in a.values() if "mean_distance" in m]
    if v: print(f"  {n}: mean {st.mean(v):.3f}  spread {within_condition_spread(a,'mean_distance'):.3f}  values {[f'{x:.2f}' for x in v]}")
