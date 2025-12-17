#!/usr/bin/env python3
"""
Telemetry integrity checker for NeuroForge.

Usage examples:
  python scripts/check_telemetry_integrity.py build/production_memory.db --out build/production_exports/integrity_report.json
  python scripts/check_telemetry_integrity.py build/production_exports --out build/production_exports/integrity_report.json

Inputs: SQLite DB path (.db/.sqlite) OR export directory containing CSVs.
Checks:
  - Expected learning_stats row count ≈ steps / memdb_interval (if available)
    Fallback: duration_ms / memdb_interval_estimated from timestamps
  - Deviation > ±5% triggers warning/fail
  - Timestamps monotonicity for learning_stats and reward_log

Outputs: JSON summary to provided --out path
"""

import argparse
import json
import os
import sqlite3
from typing import Any, Dict, List, Optional, Tuple
import glob

def try_import_pandas():
    try:
        import pandas as pd  # type: ignore
        return pd
    except Exception:
        return None

def read_csv_fallback(path: str) -> Tuple[List[Dict[str, Any]], List[str]]:
    import csv
    rows: List[Dict[str, Any]] = []
    with open(path, 'r', newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append(r)
    return rows, reader.fieldnames or []

def read_json_any_encoding(path: str) -> Dict[str, Any]:
    encodings = ('utf-8', 'utf-8-sig', 'utf-16', 'utf-16-le', 'utf-16-be')
    for enc in encodings:
        try:
            with open(path, 'r', encoding=enc) as f:
                return json.load(f)
        except Exception:
            pass
    try:
        with open(path, 'rb') as fb:
            data = fb.read()
        try:
            return json.loads(data.decode('utf-8-sig'))
        except Exception:
            return json.loads(data.decode('utf-8', errors='ignore'))
    except Exception:
        return {}

def to_float_safe(v: Any) -> Optional[float]:
    try:
        if v is None:
            return None
        if isinstance(v, (int, float)):
            return float(v)
        s = str(v).strip()
        if s == "" or s.lower() == "none":
            return None
        return float(s)
    except Exception:
        return None

def median_delta(timestamps: List[float]) -> Optional[float]:
    if len(timestamps) < 2:
        return None
    deltas = [timestamps[i] - timestamps[i-1] for i in range(1, len(timestamps))]
    deltas = [d for d in deltas if d is not None]
    if not deltas:
        return None
    deltas_sorted = sorted(deltas)
    n = len(deltas_sorted)
    mid = n // 2
    if n % 2 == 1:
        return deltas_sorted[mid]
    return (deltas_sorted[mid-1] + deltas_sorted[mid]) / 2.0

def is_monotonic_non_decreasing(timestamps: List[float]) -> bool:
    for i in range(1, len(timestamps)):
        if timestamps[i] < timestamps[i-1]:
            return False
    return True

def find_column(candidates: List[str], available: List[str]) -> Optional[str]:
    for c in candidates:
        if c in available:
            return c
    return None

def pick_csv(base_dir: str, base_name: str) -> Optional[str]:
    patterns = [
        os.path.join(base_dir, f"{base_name}.csv"),
        os.path.join(base_dir, f"{base_name}_run_*.csv"),
        os.path.join(base_dir, f"{base_name}*.csv"),
    ]
    candidates: List[str] = []
    for p in patterns:
        candidates.extend(glob.glob(p))
    if not candidates:
        return None
    candidates = sorted(candidates, key=lambda p: os.path.getmtime(p), reverse=True)
    return candidates[0]

def load_from_dir(export_dir: str) -> Dict[str, Any]:
    pd = try_import_pandas()
    result: Dict[str, Any] = {
        "source": "dir",
        "path": export_dir,
        "learning_stats_rows": 0,
        "reward_log_rows": 0,
        "steps": None,
        "memdb_interval_ms": None,
        "reward_interval_ms": None,
        "timestamps_monotonic": {"learning_stats": None, "reward_log": None},
        "expected_learning_rows": None,
        "deviation_percent": None,
        "passed": False,
        "method": None,
    }

    ls_path = pick_csv(export_dir, "learning_stats")
    rw_path = pick_csv(export_dir, "reward_log")
    # Prefer explicit run meta; avoid reading integrity_report.json to prevent recursion
    candidate_meta = os.path.join(export_dir, "run_meta.json")
    meta_path = candidate_meta if os.path.exists(candidate_meta) else None

    ls_rows: List[Dict[str, Any]] = []
    rw_rows: List[Dict[str, Any]] = []
    ls_cols: List[str] = []
    rw_cols: List[str] = []

    if ls_path and os.path.exists(ls_path):
        if pd:
            df = pd.read_csv(ls_path)
            ls_rows = df.to_dict(orient="records")
            ls_cols = list(df.columns)
        else:
            ls_rows, ls_cols = read_csv_fallback(ls_path)
    if rw_path and os.path.exists(rw_path):
        if pd:
            df = pd.read_csv(rw_path)
            rw_rows = df.to_dict(orient="records")
            rw_cols = list(df.columns)
        else:
            rw_rows, rw_cols = read_csv_fallback(rw_path)

    result["learning_stats_rows"] = len(ls_rows)
    result["reward_log_rows"] = len(rw_rows)

    # Attempt to derive intervals from timestamp deltas
    ts_col_ls = find_column(["timestamp_ms", "ts_ms", "ts", "time_ms", "time"], ls_cols)
    ts_col_rw = find_column(["timestamp_ms", "ts_ms", "ts", "time_ms", "time"], rw_cols)

    ls_ts = [to_float_safe(r.get(ts_col_ls)) for r in ls_rows] if ts_col_ls else []
    ls_ts = [t for t in ls_ts if t is not None]
    rw_ts = [to_float_safe(r.get(ts_col_rw)) for r in rw_rows] if ts_col_rw else []
    rw_ts = [t for t in rw_ts if t is not None]

    memdb_interval_est = median_delta(ls_ts) if ls_ts else None
    reward_interval_est = median_delta(rw_ts) if rw_ts else None
    result["memdb_interval_ms"] = memdb_interval_est
    result["reward_interval_ms"] = reward_interval_est

    # Monotonicity
    result["timestamps_monotonic"]["learning_stats"] = is_monotonic_non_decreasing(ls_ts) if ls_ts else None
    result["timestamps_monotonic"]["reward_log"] = is_monotonic_non_decreasing(rw_ts) if rw_ts else None

    # Steps if available
    step_col = find_column(["step", "steps"], ls_cols)
    steps_val: Optional[int] = None
    if step_col and ls_rows:
        vals = [r.get(step_col) for r in ls_rows]
        steps_val = None
        try:
            steps_val = int(max(int(v) for v in vals if v is not None))
        except Exception:
            steps_val = None
    result["steps"] = steps_val

    # Merge meta early and override known fields when available
    meta_loaded = False
    if meta_path and os.path.exists(meta_path):
        try:
            meta = read_json_any_encoding(meta_path)
            result["meta"] = meta
            meta_loaded = True
            # Recognize multiple key aliases
            for key_pair in [
                ("memdb_interval_ms", ["memdb_interval_ms", "telemetry_interval_ms", "mem_interval_ms", "memdb_interval"]),
                ("reward_interval_ms", ["reward_interval_ms", "reward_interval", "reward_tick_ms"]),
                ("steps", ["steps", "total_steps", "run_steps"]),
            ]:
                dest, aliases = key_pair
                for a in aliases:
                    if a in meta and meta[a] is not None:
                        try:
                            if dest == "steps":
                                result[dest] = int(meta[a])
                            else:
                                result[dest] = float(meta[a])
                            break
                        except Exception:
                            pass
        except Exception:
            result["meta"] = {"error": f"Failed to read {os.path.basename(meta_path)}"}

    # Expected rows calculation prioritizing meta values when present
    expected_rows: Optional[float] = None
    method: Optional[str] = None
    deviation_percent: Optional[float] = None

    steps_val = result.get("steps")
    memdb_interval_ms = result.get("memdb_interval_ms")
    reward_interval_ms = result.get("reward_interval_ms")

    if steps_val is not None and memdb_interval_ms and memdb_interval_ms > 0:
        if reward_interval_ms and reward_interval_ms > 0:
            # Expectation driven by meta: rows ≈ steps * (memdb_interval_ms / reward_interval_ms)
            expected_rows = int(steps_val * (memdb_interval_ms / reward_interval_ms))
        else:
            # Simpler fallback: rows ≈ steps / (1000 / memdb_interval_ms)
            expected_rows = int(steps_val / (1000.0 / memdb_interval_ms))
        method = "meta" if meta_loaded else "db"
    elif memdb_interval_ms and ls_ts:
        duration_ms = ls_ts[-1] - ls_ts[0]
        if duration_ms > 0:
            expected_rows = duration_ms / memdb_interval_ms
            method = "time_based"

    actual_rows = len(ls_rows)
    if expected_rows is not None and expected_rows > 0:
        deviation_percent = 100.0 * abs(actual_rows - expected_rows) / expected_rows
        result["expected_learning_rows"] = int(expected_rows)
        result["deviation_percent"] = round(deviation_percent, 3)
        result["method"] = method
        result["passed"] = deviation_percent <= 5.0
    else:
        result["expected_learning_rows"] = None
        result["deviation_percent"] = None
        result["method"] = method
        result["passed"] = False

    return result

def load_from_db(db_path: str) -> Dict[str, Any]:
    pd = try_import_pandas()
    result: Dict[str, Any] = {
        "source": "db",
        "path": db_path,
        "learning_stats_rows": 0,
        "reward_log_rows": 0,
        "steps": None,
        "memdb_interval_ms": None,
        "reward_interval_ms": None,
        "timestamps_monotonic": {"learning_stats": None, "reward_log": None},
        "expected_learning_rows": None,
        "deviation_percent": None,
        "passed": False,
        "method": None,
    }

    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()

    def table_exists(name: str) -> bool:
        cur.execute("SELECT name FROM sqlite_master WHERE type='table' AND name=?", (name,))
        return cur.fetchone() is not None

    ls_rows: List[Dict[str, Any]] = []
    rw_rows: List[Dict[str, Any]] = []

    if table_exists("learning_stats"):
        cur.execute("SELECT * FROM learning_stats")
        ls_rows = [dict(r) for r in cur.fetchall()]
    if table_exists("reward_log"):
        cur.execute("SELECT * FROM reward_log")
        rw_rows = [dict(r) for r in cur.fetchall()]

    result["learning_stats_rows"] = len(ls_rows)
    result["reward_log_rows"] = len(rw_rows)

    # Optional runs table for steps/intervals
    runs_row = None
    if table_exists("runs"):
        cur.execute("SELECT * FROM runs ORDER BY id DESC LIMIT 1")
        r = cur.fetchone()
        runs_row = dict(r) if r else None
    if runs_row:
        for k in ("steps", "memdb_interval_ms", "reward_interval_ms"):
            if k in runs_row:
                val = runs_row.get(k)
                try:
                    result[k] = int(val) if k == "steps" else float(val)
                except Exception:
                    result[k] = None

    # Timestamp columns inference
    ls_cols = list(ls_rows[0].keys()) if ls_rows else []
    rw_cols = list(rw_rows[0].keys()) if rw_rows else []
    ts_col_ls = find_column(["timestamp_ms", "ts", "time_ms", "time"], ls_cols)
    ts_col_rw = find_column(["timestamp_ms", "ts", "time_ms", "time"], rw_cols)
    ls_ts = [to_float_safe(r.get(ts_col_ls)) for r in ls_rows] if ts_col_ls else []
    ls_ts = [t for t in ls_ts if t is not None]
    rw_ts = [to_float_safe(r.get(ts_col_rw)) for r in rw_rows] if ts_col_rw else []
    rw_ts = [t for t in rw_ts if t is not None]

    memdb_interval_est = median_delta(ls_ts) if ls_ts else None
    reward_interval_est = median_delta(rw_ts) if rw_ts else None
    if result["memdb_interval_ms"] is None:
        result["memdb_interval_ms"] = memdb_interval_est
    if result["reward_interval_ms"] is None:
        result["reward_interval_ms"] = reward_interval_est

    result["timestamps_monotonic"]["learning_stats"] = is_monotonic_non_decreasing(ls_ts) if ls_ts else None
    result["timestamps_monotonic"]["reward_log"] = is_monotonic_non_decreasing(rw_ts) if rw_ts else None

    steps_val = result.get("steps")
    expected_rows: Optional[float] = None
    method: Optional[str] = None

    memdb_interval_ms = result.get("memdb_interval_ms")
    if steps_val is not None and memdb_interval_ms and memdb_interval_ms > 0:
        expected_rows = steps_val / memdb_interval_ms
        method = "steps_based"
    elif memdb_interval_ms and ls_ts:
        duration_ms = ls_ts[-1] - ls_ts[0]
        if duration_ms > 0:
            expected_rows = duration_ms / memdb_interval_ms
            method = "time_based"

    actual_rows = len(ls_rows)
    if expected_rows and expected_rows > 0:
        deviation_percent = abs(actual_rows - expected_rows) / expected_rows * 100.0
        result["expected_learning_rows"] = round(expected_rows, 3)
        result["deviation_percent"] = round(deviation_percent, 3)
        result["method"] = method
        result["passed"] = deviation_percent <= 5.0
    else:
        result["expected_learning_rows"] = None
        result["deviation_percent"] = None
        result["method"] = method
        result["passed"] = False

    conn.close()
    return result

def main():
    parser = argparse.ArgumentParser(description="Check telemetry integrity for NeuroForge")
    parser.add_argument("input", help="SQLite DB path or export directory")
    parser.add_argument("--out", required=True, help="Output JSON path")
    args = parser.parse_args()

    in_path = args.input
    out_path = args.out
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    if os.path.isdir(in_path):
        report = load_from_dir(in_path)
    elif os.path.isfile(in_path):
        lower = in_path.lower()
        if lower.endswith(".db") or lower.endswith(".sqlite"):
            report = load_from_db(in_path)
        else:
            raise SystemExit(f"Unsupported file type: {in_path}")
    else:
        raise SystemExit(f"Input not found: {in_path}")

    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=2)
    print(f"Integrity report written: {out_path}")

if __name__ == "__main__":
    main()
