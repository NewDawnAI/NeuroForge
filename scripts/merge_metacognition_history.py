#!/usr/bin/env python3
import argparse
import glob
import json
import os
from datetime import datetime, timezone
from typing import Any, Dict, List, Tuple

# Helper to parse timestamps flexibly
TS_KEYS = ["ts_ms", "timestamp", "created_at", "ts", "time_ms"]


def extract_rows(payload: Dict[str, Any]) -> List[Dict[str, Any]]:
    rows = payload.get("metacognition") or payload.get("rows") or []
    return rows if isinstance(rows, list) else []


def get_ts_ms(row: Dict[str, Any]) -> int:
    for k in TS_KEYS:
        v = row.get(k)
        if v is None:
            continue
        try:
            return int(v)
        except Exception:
            # try parse ISO string
            try:
                dt = datetime.fromisoformat(str(v).replace("Z", "+00:00"))
                return int(dt.timestamp() * 1000)
            except Exception:
                continue
    return 0


def get_trust(row: Dict[str, Any]) -> float:
    v = row.get("self_trust")
    try:
        return float(v) if v is not None else float("nan")
    except Exception:
        return float("nan")


def iso_week_key(ts_ms: int) -> Tuple[int, int]:
    dt = datetime.fromtimestamp(ts_ms / 1000.0, tz=timezone.utc)
    iso = dt.isocalendar()  # year, week, weekday
    return (iso[0], iso[1])


def merge_history(input_dir: str, pattern: str) -> Dict[str, Any]:
    files = sorted(glob.glob(os.path.join(input_dir, pattern)))
    all_rows: List[Dict[str, Any]] = []
    for fp in files:
        try:
            with open(fp, "r", encoding="utf-8") as f:
                payload = json.load(f)
            rows = extract_rows(payload)
            all_rows.extend(rows)
        except Exception:
            continue
    # Sort by timestamp
    all_rows.sort(key=lambda r: get_ts_ms(r))
    # Compute weekly averages
    weekly: Dict[Tuple[int,int], List[float]] = {}
    for r in all_rows:
        ts_ms = get_ts_ms(r)
        trust = get_trust(r)
        if trust != trust:  # NaN check
            continue
        key = iso_week_key(ts_ms)
        weekly.setdefault(key, []).append(trust)
    weekly_series = []
    for (year, week) in sorted(weekly.keys()):
        vals = weekly[(year, week)]
        avg = sum(vals) / len(vals) if vals else 0.0
        weekly_series.append({"year": year, "week": week, "mean_self_trust": avg, "count": len(vals)})
    return {"weekly_self_trust": weekly_series, "source_count": len(files)}


def main() -> int:
    ap = argparse.ArgumentParser(description="Merge nightly metacognition exports and compute weekly trust stability")
    ap.add_argument("--in", dest="input_dir", default="web", help="Input directory containing JSON exports")
    ap.add_argument("--pattern", default="metacognition_export*.json", help="Glob pattern for export files")
    ap.add_argument("--out", default="web/metacognition_weekly.json", help="Output JSON for weekly stability")
    args = ap.parse_args()

    result = merge_history(args.input_dir, args.pattern)
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)
    print(f"Wrote weekly stability to {args.out} (weeks={len(result['weekly_self_trust'])}, sources={result['source_count']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
