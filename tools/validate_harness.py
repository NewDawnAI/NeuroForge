#!/usr/bin/env python3
"""Validate nf_experiment against one known positive and one known null.

A harness that cannot reproduce a result already established by hand is not
trustworthy, and a harness that finds an effect everywhere is worse than none. So
it is checked against both directions:

  POSITIVE  --learn-policy    solves phototaxis by hand-analysis (2.139 -> 0.967)
  NULL      --selection-smoothing=0.1   measured as +/-0.07 with no dose-response

Eight seeds, not the three used during the session: floor_p(3)=0.25 and
floor_p(5)=0.0625, so p<0.05 is UNREACHABLE below six seeds. At eight the floor
is 0.0078 and the test can actually resolve something.

The arms also differ from the session's design in a way that matters: they share
a set of DISTINCT seeds and are paired per seed, rather than repeating one seed
and relying on residual nondeterminism for spread. Repeats at a fixed seed
measure the implementation's noise floor; different seeds measure whether an
effect survives different initial conditions.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from nf_experiment import (CLOSED_LOOP, Arm, check_metric_moves, floor_p,
                           report, run_arm, within_condition_spread)

SEEDS = [401, 402, 403, 404, 405, 406, 407, 408]
STEPS = 1500

# Each arm is validated in the configuration its effect was ESTABLISHED in.
#
# The first version of this script ran both arms under one shared config that
# included --motor-selection --action-credit, at 900 steps. That changed three
# variables at once relative to how the policy result was measured, and duly
# reported the known positive as a null (+0.153, 3/8 wins). The result was fine;
# the check was miscontrolled. Re-run in the original configuration the same
# comparison gives 8/8 wins, p at the floor (0.00781), effect/sem 9.34.
#
# The lesson is the one this harness exists to enforce: a control differing from
# its arm in more than one respect cannot attribute anything.
POLICY_COMMON = [*CLOSED_LOOP, f'--steps={STEPS}']
SUBSTRATE_COMMON = [*CLOSED_LOOP, f'--steps={STEPS}',
                    '--motor-selection', '--action-credit']


def main() -> int:
    print(f"seeds={SEEDS}  n={len(SEEDS)}  floor_p={floor_p(len(SEEDS)):.5f}  steps={STEPS}")
    print("(p<0.05 is unreachable below 6 seeds; the session used 3)\n")

    print('-- POSITIVE pair (policy config: argmax over input regions)')
    p_control = run_arm(Arm('argmax', []), SEEDS, POLICY_COMMON)
    learned = run_arm(Arm('learned', ['--learn-policy']), SEEDS, POLICY_COMMON)

    print('')
    print('-- NULL pair (substrate config: motor selection + action credit)')
    s_control = run_arm(Arm('substrate', []), SEEDS, SUBSTRATE_COMMON)
    smoothed = run_arm(Arm('smoothed', ['--selection-smoothing=0.1']), SEEDS,
                       SUBSTRATE_COMMON)

    # Lower distance is better: the agent is trying to reach the light.
    report("KNOWN POSITIVE — learned policy", p_control, learned,
           "mean_distance", higher_is_better=False)
    report("KNOWN NULL — selection smoothing", s_control, smoothed,
           "mean_distance", higher_is_better=False)

    print("\n=== manipulation checks ===")
    for label, base, arm in (("learned", p_control, learned),
                             ("smoothed", s_control, smoothed)):
        chk = check_metric_moves(base, arm, "n_decision_changes")
        if chk is not None:
            status = "PASS" if chk.passed else "FAIL"
            print(f"  {label}: decision-change count moved? {status} — {chk.detail}")

    print("\n=== within-condition spreads (the number every retraction lacked) ===")
    for label, arm in (("argmax", p_control), ("learned", learned),
                       ("substrate", s_control), ("smoothed", smoothed)):
        print(f"  {label:9s} mean_distance spread = "
              f"{within_condition_spread(arm, 'mean_distance'):.4f}")

    print("\nHarness is trustworthy only if the positive resolves AND the null does not.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
