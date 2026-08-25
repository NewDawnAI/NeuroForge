#!/usr/bin/env python3
"""Does the learned policy use vision once the task requires it?

On the fixed world the advantage survived ablating VisualCortex almost unchanged
(-0.644 intact vs -0.625 ablated), so the policy was solving the task without
perceiving. --task-randomize moves the light and the agent every N cycles, which
no fixed action bias survives.

The diagnostic is the DIFFERENCE OF DIFFERENCES: if the learned advantage is
large intact and collapses under ablation, the policy is now using vision.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from nf_experiment import CLOSED_LOOP, Arm, floor_p, report, run_arm

SEEDS = [401, 402, 403, 404, 405, 406]
COMMON = [*CLOSED_LOOP, "--steps=1500", "--task=photo", "--task-randomize=40"]
ABL = ["--ablate=zero", "--ablate-region=VisualCortex"]

print(f"randomised every 40 cycles  n={len(SEEDS)}  floor_p={floor_p(len(SEEDS)):.5f}\n")
ctl     = run_arm(Arm("control"),         SEEDS, COMMON)
lrn     = run_arm(Arm("learned"),         SEEDS, COMMON + ["--learn-policy"])
ctl_abl = run_arm(Arm("control+abl"),     SEEDS, COMMON + ABL)
lrn_abl = run_arm(Arm("learned+abl"),     SEEDS, COMMON + ABL + ["--learn-policy"])

report("randomised, intact", ctl, lrn, "mean_distance", higher_is_better=False)
report("randomised, VISION ABLATED", ctl_abl, lrn_abl, "mean_distance",
       higher_is_better=False)
print("\nfixed-world reference: intact -0.644, ablated -0.625 (vision ~3% of effect)")
