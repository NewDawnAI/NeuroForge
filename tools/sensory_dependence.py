#!/usr/bin/env python3
"""Does the learned policy actually USE the sensory pathway?

A quick check found that ablating VisualCortex left photo-task performance
unchanged (1.33 -> 1.33), while ablating every region moved it (1.33 -> 1.71).
If the task is solvable without the sensory cortex that carries the cue, then
"cross-modal transfer" is not a measurable question here -- the second task would
differ from the first in a variable neither of them depends on.

Four arms, 6 seeds, paired. The comparison that matters is whether the learned
policy's advantage over control SURVIVES ablating the cortex carrying the cue.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from nf_experiment import CLOSED_LOOP, Arm, floor_p, report, run_arm

SEEDS = [401, 402, 403, 404, 405, 406]
COMMON = [*CLOSED_LOOP, "--steps=900", "--task=photo"]
ABL = ["--ablate=zero", "--ablate-region=VisualCortex"]

print(f"n={len(SEEDS)}  floor_p={floor_p(len(SEEDS)):.5f}\n")
ctl      = run_arm(Arm("control"),          SEEDS, COMMON)
lrn      = run_arm(Arm("learned"),          SEEDS, COMMON + ["--learn-policy"])
ctl_abl  = run_arm(Arm("control+ablated"),  SEEDS, COMMON + ABL)
lrn_abl  = run_arm(Arm("learned+ablated"),  SEEDS, COMMON + ABL + ["--learn-policy"])

report("intact: learned vs control", ctl, lrn, "mean_distance", higher_is_better=False)
report("VISION ABLATED: learned vs control", ctl_abl, lrn_abl, "mean_distance",
       higher_is_better=False)
print("\nIf the advantage survives ablation, the policy is not using vision and")
print("cross-modal transfer is not a measurable question on this task.")
