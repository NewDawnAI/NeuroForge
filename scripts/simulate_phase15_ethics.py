#!/usr/bin/env python3
"""
Simulate Phase 15 Ethics Regulator decisions and emit a dashboard JSON file.

Generates synthetic decisions (allow/review/deny) based on a risk series and
writes to web/ethics_regulator_log.json by default, matching the dashboard shape.

Usage (Windows PowerShell):
  python .\scripts\simulate_phase15_ethics.py --out .\web\ethics_regulator_log.json \
    --events 150 --risk-threshold 0.30 --window 50 --seed 42

Notes:
  - Decision rule: allow if risk <= threshold-0.05; review in (threshold-0.05, threshold+0.05); deny if >= threshold+0.05
  - Timestamps are spaced uniformly backwards from now for a readable timeline
"""

import argparse
import json
import math
import os
import random
import time
from typing import List, Dict


def decide(risk: float, threshold: float) -> str:
    margin = 0.05
    if risk <= max(0.0, threshold - margin):
        return "allow"
    if risk >= min(1.0, threshold + margin):
        return "deny"
    return "review"


def generate_series(n: int, threshold: float, window: int, seed: int = None) -> List[Dict]:
    rng = random.Random(seed)
    now_ms = int(time.time() * 1000)
    step_ms = 20_000  # 20s between entries for a clear timeline

    series: List[Dict] = []

    # Base risk components: slow wave + noise + occasional spikes
    spike_every = max(10, window // 2)
    for i in range(n):
        t_ms = now_ms - (n - 1 - i) * step_ms

        # Smooth component
        phase = (i / max(1, window)) * 2 * math.pi
        smooth = 0.2 + 0.15 * math.sin(phase)

        # Noise
        noise = rng.uniform(-0.08, 0.08)

        # Occasional spike
        spike = 0.0
        if i % spike_every == 0 and i > 0:
            spike = rng.uniform(0.15, 0.35)

        # Combine and clamp
        risk = max(0.0, min(1.0, smooth + noise + spike))

        decision = decide(risk, threshold)

        note = {
            "allow": "low risk window",
            "review": "near threshold",
            "deny": "spike in error",
        }[decision]

        # Minimal context for tooltips
        context = {
            "coherence": round(0.4 + rng.uniform(-0.1, 0.2), 3),
            "goal_mae": round(abs(rng.gauss(0.12, 0.05)), 3),
            "window": window,
            "threshold": threshold,
        }

        series.append({
            "ts_ms": t_ms,
            "decision": decision,
            "risk": round(risk, 3),
            "notes": note,
            "context": context,
        })

    return series


def summarize(series: List[Dict]) -> Dict[str, int]:
    counts = {"allow": 0, "review": 0, "deny": 0}
    for row in series:
        counts[row["decision"]] += 1
    return counts


def main():
    ap = argparse.ArgumentParser(description="Simulate Phase 15 ethics decisions and emit dashboard JSON")
    ap.add_argument("--out", default=os.path.join("web", "ethics_regulator_log.json"), help="Output JSON path (default web/ethics_regulator_log.json)")
    ap.add_argument("--events", type=int, default=120, help="Number of decision events to generate")
    ap.add_argument("--risk-threshold", type=float, default=0.30, help="Risk threshold in [0,1]")
    ap.add_argument("--window", type=int, default=50, help="Window length used in context and spike cadence")
    ap.add_argument("--seed", type=int, default=None, help="Random seed for reproducibility")
    args = ap.parse_args()

    series = generate_series(args.events, args.risk_threshold, args.window, args.seed)
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(series, f, indent=2)

    counts = summarize(series)
    print(f"Wrote {len(series)} ethics decisions to {args.out}")
    print(f"Counts: allow={counts['allow']}, review={counts['review']}, deny={counts['deny']}")
    print("Example row:")
    print(json.dumps(series[min(5, len(series)-1)], indent=2))


if __name__ == "__main__":
    main()

