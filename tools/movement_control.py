#!/usr/bin/env python3
"""Is the learned advantage perception, or just movement?

On the randomised task the learned policy beats control by -0.911, and -0.733 of
that survives ablating VisualCortex. So ~80% is not visual. The obvious
alternative is that the learned policy simply MOVES more -- its decision-change
count is 6.96x control's -- and moving beats holding when distance is averaged.

--policy-temp=100 makes the softmax near-uniform: the policy still samples
actions and still moves, but its weights barely influence the choice. If that
arm captures most of the advantage, the effect is movement, not learning.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from nf_experiment import CLOSED_LOOP, Arm, floor_p, report, run_arm

SEEDS = [401, 402, 403, 404, 405, 406]
COMMON = [*CLOSED_LOOP, "--steps=1500", "--task=photo", "--task-randomize=40"]

print(f"n={len(SEEDS)}  floor_p={floor_p(len(SEEDS)):.5f}\n")
ctl    = run_arm(Arm("control-argmax"), SEEDS, COMMON)
rand   = run_arm(Arm("near-random"),    SEEDS, COMMON + ["--learn-policy", "--policy-temp=100"])
learn  = run_arm(Arm("learned"),        SEEDS, COMMON + ["--learn-policy"])

report("movement alone (near-random policy) vs control", ctl, rand,
       "mean_distance", higher_is_better=False)
report("learned vs control", ctl, learn, "mean_distance", higher_is_better=False)
report("learned vs near-random  <-- the LEARNING, net of movement", rand, learn,
       "mean_distance", higher_is_better=False)
