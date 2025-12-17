#!/usr/bin/env python3
"""
Generate a synthetic hysteresis trajectory for Phase 15 ethics decisions.

Emits a JSON with an `ethics_regulator_log` list where the threshold changes
mid-run according to the provided segments, e.g. "0.2:1500,0.4:1500,0.3:1500".

Usage (Windows PowerShell):
  python scripts/generate_hysteresis.py --out web/hysteresis_0.2_0.4_0.3.json \
    --segments "0.2:1500,0.4:1500,0.3:1500" --window 50 --seed 42

Notes:
  - Decision rule mirrors the simulator: allow if risk <= threshold-0.05;
    review in (threshold-0.05, threshold+0.05); deny if >= threshold+0.05
  - Risk values are sampled around each segment's threshold with mild noise
  - Timestamps are spaced uniformly backwards from now for readability
"""

import argparse
import json
import math
import os
import random
import time
from typing import List, Dict, Tuple


def parse_segments(spec: str) -> List[Tuple[float, int]]:
    parts = [p.strip() for p in spec.split(",") if p.strip()]
    segs = []
    for p in parts:
        thr_str, count_str = p.split(":")
        segs.append((float(thr_str), int(count_str)))
    return segs


def gen_decision(risk: float, thr: float) -> str:
    if risk <= thr - 0.05:
        return "allow"
    if risk >= thr + 0.05:
        return "deny"
    return "review"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="Output JSON path (e.g., web/hysteresis_0.2_0.4_0.3.json)")
    ap.add_argument("--segments", required=True, help="Comma-separated segments 'thr:count,...'")
    ap.add_argument("--window", type=int, default=50)
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    random.seed(args.seed)
    segments = parse_segments(args.segments)

    now_ms = int(time.time() * 1000)
    dt_ms = 2000
    ts = now_ms
    logs: List[Dict] = []

    for thr, count in segments:
        for _ in range(count):
            # Sample risk around threshold with mild noise (Gaussian clipped to [0,1])
            risk = max(0.0, min(1.0, random.gauss(mu=thr, sigma=0.08)))
            dec = gen_decision(risk, thr)
            entry = {
                "ts_ms": ts,
                "decision": dec,
                "risk": round(risk, 3),
                "notes": "hysteresis segment",
                "context": {
                    "coherence": round(random.random(), 3),
                    "goal_mae": round(random.random() * 0.25, 3),
                    "window": args.window,
                    "threshold": thr,
                },
            }
            logs.append(entry)
            ts += dt_ms

    out = {"ethics_regulator_log": logs}
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
    print(f"Wrote hysteresis log to {args.out} (segments={len(segments)}, total={len(logs)})")


if __name__ == "__main__":
    main()

