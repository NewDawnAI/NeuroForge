#!/usr/bin/env python3
"""
Warning regression gate for NeuroForge CI.

Usage examples:
  python tools/check_warning_regression.py --build-log build.log --write-baseline .ci/warnings-baseline.json
  python tools/check_warning_regression.py --build-log build.log --baseline .ci/warnings-baseline.json
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

WARNING_PATTERNS = [
    re.compile(r"warning:"),             # GCC/Clang
    re.compile(r"\bwarning C\d+\b"),   # MSVC
]


def is_warning_line(line: str) -> bool:
    s = line.strip()
    return any(p.search(s) for p in WARNING_PATTERNS)


def classify_warning(line: str) -> str:
    # Attempt to classify into stable buckets for trend tracking.
    # GCC/Clang often include [-Wfoo] at end.
    m = re.search(r"\[-W([A-Za-z0-9\-]+)\]", line)
    if m:
        return f"gcc_clang:{m.group(1)}"
    m = re.search(r"\bwarning (C\d+)\b", line)
    if m:
        return f"msvc:{m.group(1)}"
    return "unclassified"


def parse_build_log(path: Path) -> Counter:
    counts: Counter[str] = Counter()
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if is_warning_line(line):
            counts[classify_warning(line)] += 1
    return counts


def to_sorted_dict(counter: Counter) -> dict[str, int]:
    return {k: counter[k] for k in sorted(counter)}


def main() -> int:
    ap = argparse.ArgumentParser(description="Check compiler warning regressions from a build log")
    ap.add_argument("--build-log", required=True, type=Path, help="Path to compiler/build output log")
    ap.add_argument("--baseline", type=Path, help="Path to baseline JSON to compare against")
    ap.add_argument("--write-baseline", type=Path, help="Write/update baseline JSON and exit success")
    ap.add_argument("--allow-unclassified-growth", action="store_true", help="Ignore growth of unclassified warnings")
    args = ap.parse_args()

    if not args.build_log.exists():
        print(f"[warn-gate] ERROR: build log does not exist: {args.build_log}")
        return 2

    current = parse_build_log(args.build_log)
    current_data = {
        "total": int(sum(current.values())),
        "by_type": to_sorted_dict(current),
    }

    if args.write_baseline:
        args.write_baseline.parent.mkdir(parents=True, exist_ok=True)
        args.write_baseline.write_text(json.dumps(current_data, indent=2) + "\n", encoding="utf-8")
        print(f"[warn-gate] Wrote baseline: {args.write_baseline} (total={current_data['total']})")
        return 0

    if not args.baseline:
        print("[warn-gate] ERROR: provide --baseline for comparison or --write-baseline to create one")
        return 2

    if not args.baseline.exists():
        print(f"[warn-gate] ERROR: baseline does not exist: {args.baseline}")
        return 2

    baseline_data = json.loads(args.baseline.read_text(encoding="utf-8"))
    baseline_counter = Counter(baseline_data.get("by_type", {}))

    regressions: list[tuple[str, int, int]] = []
    for key in sorted(set(current) | set(baseline_counter)):
        before = int(baseline_counter.get(key, 0))
        after = int(current.get(key, 0))
        if args.allow_unclassified_growth and key == "unclassified":
            continue
        if after > before:
            regressions.append((key, before, after))

    print(f"[warn-gate] Baseline total={baseline_data.get('total', 0)} current total={current_data['total']}")

    if regressions:
        print("[warn-gate] WARNING REGRESSIONS DETECTED:")
        for key, before, after in regressions:
            print(f"  - {key}: {before} -> {after} (+{after-before})")
        return 1

    print("[warn-gate] OK: no warning-type regressions detected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
