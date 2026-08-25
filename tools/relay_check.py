#!/usr/bin/env python3
"""Does the thalamic relay carry the visual signal the policy uses?

Ablating VisualCortex left ~80% of the learned advantage intact, which I read as
"the policy is not using vision". That reading was wrong. With --learn-policy the
policy has SIX input channels: four from Thalamus/Hippocampus/Amygdala/Cingulate
and two direct VisualCortex features. VisualCortex projects to Thalamus, and
Thalamus was not ablated -- so zeroing VisualCortex removes two of six channels
while the relay keeps carrying the bearing.

If that is right, ablating THALAMUS should cost more than ablating VisualCortex,
because it severs the relay every downstream channel depends on.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from nf_experiment import CLOSED_LOOP, Arm, floor_p, report, run_arm

SEEDS = [401, 402, 403, 404, 405, 406]
COMMON = [*CLOSED_LOOP, "--steps=1500", "--task=photo", "--task-randomize=40"]
L = ["--learn-policy"]

print(f"n={len(SEEDS)}  floor_p={floor_p(len(SEEDS)):.5f}\n")
ctl      = run_arm(Arm("control"),      SEEDS, COMMON)
lrn      = run_arm(Arm("learned"),      SEEDS, COMMON + L)
lrn_thal = run_arm(Arm("learned-thal"), SEEDS,
                   COMMON + L + ["--ablate=zero", "--ablate-region=Thalamus"])

report("intact", ctl, lrn, "mean_distance", higher_is_better=False)
report("THALAMUS ablated", ctl, lrn_thal, "mean_distance", higher_is_better=False)
print("\nreference on this task: VisualCortex ablated gave -0.733 vs -0.911 intact")
