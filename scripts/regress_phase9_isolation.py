#!/usr/bin/env python3
import argparse
import json
import math
import sqlite3
import statistics
from datetime import datetime
from typing import Any, Dict, Optional


def table_exists(conn: sqlite3.Connection, table: str) -> bool:
    cur = conn.cursor()
    cur.execute("SELECT name FROM sqlite_master WHERE type='table' AND name = ?;", (table,))
    return cur.fetchone() is not None


def latest_run_id(conn: sqlite3.Connection) -> Optional[int]:
    cur = conn.cursor()
    cur.execute("SELECT MAX(id) FROM runs;")
    row = cur.fetchone()
    return int(row[0]) if row and row[0] is not None else None


def get_run_id(conn: sqlite3.Connection, run_id: Optional[int]) -> Optional[int]:
    if run_id is not None:
        return run_id
    return latest_run_id(conn)


def fetch_rewards(conn: sqlite3.Connection, run_id: int) -> list[float]:
    if not table_exists(conn, "reward_log"):
        return []
    cur = conn.cursor()
    cur.execute("SELECT reward FROM reward_log WHERE run_id = ? ORDER BY ts_ms ASC;", (run_id,))
    return [float(r[0]) for r in cur.fetchall() if r and r[0] is not None]


def fetch_reflections_count(conn: sqlite3.Connection, run_id: int) -> int:
    if not table_exists(conn, "reflections"):
        return 0
    cur = conn.cursor()
    cur.execute("SELECT COUNT(*) FROM reflections WHERE run_id = ?;", (run_id,))
    row = cur.fetchone()
    return int(row[0]) if row and row[0] is not None else 0


def fetch_trust_series(conn: sqlite3.Connection, run_id: int) -> list[tuple[int, float]]:
    if not table_exists(conn, "metacognition"):
        return []
    cur = conn.cursor()
    cur.execute("SELECT ts_ms, self_trust FROM metacognition WHERE run_id = ? AND self_trust IS NOT NULL ORDER BY ts_ms ASC;", (run_id,))
    rows = cur.fetchall()
    return [(int(r[0]), float(r[1])) for r in rows if r and r[0] is not None and r[1] is not None]


def trust_drift_per_hour(series: list[tuple[int, float]]) -> float:
    if len(series) < 2:
        return 0.0
    t0, v0 = series[0]
    t1, v1 = series[-1]
    dt_hours = max(1e-6, (t1 - t0) / 3600000.0)
    return (v1 - v0) / dt_hours


def isolation_checks(conn: sqlite3.Connection, run_id: int) -> Dict[str, Any]:
    meta_count = 0
    pred_res_count = 0
    if table_exists(conn, "metacognition"):
        cur = conn.cursor()
        cur.execute("SELECT COUNT(*) FROM metacognition WHERE run_id = ?;", (run_id,))
        meta_count = int(cur.fetchone()[0])
    if table_exists(conn, "prediction_resolutions"):
        cur = conn.cursor()
        cur.execute("SELECT COUNT(*) FROM prediction_resolutions WHERE run_id = ?;", (run_id,))
        pred_res_count = int(cur.fetchone()[0])
    return {
        "metacognition_rows": meta_count,
        "prediction_resolutions_rows": pred_res_count,
        "isolated": (meta_count == 0 and pred_res_count == 0),
    }


def summarize_run(conn: sqlite3.Connection, run_id: int) -> Dict[str, Any]:
    rewards = fetch_rewards(conn, run_id)
    reflections = fetch_reflections_count(conn, run_id)
    trust_series = fetch_trust_series(conn, run_id)
    out: Dict[str, Any] = {
        "run_id": run_id,
        "reward_count": len(rewards),
        "reward_volatility_std": (statistics.pstdev(rewards) if len(rewards) > 1 else 0.0),
        "reflections_count": reflections,
        "trust_drift_per_hour": trust_drift_per_hour(trust_series),
    }
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Regress Phase 9 isolation: compare baseline vs metacog-enabled runs")
    ap.add_argument("--baseline_db", required=True, help="Path to baseline SQLite (phase9=off)")
    ap.add_argument("--meta_db", required=True, help="Path to metacog SQLite (phase9=on)")
    ap.add_argument("--baseline_run_id", type=int, default=None, help="Baseline run id; default latest")
    ap.add_argument("--meta_run_id", type=int, default=None, help="Meta run id; default latest")
    ap.add_argument("--out", default="web/regression_phase9_isolation.json", help="Output JSON path")
    args = ap.parse_args()

    bconn = sqlite3.connect(args.baseline_db)
    mconn = sqlite3.connect(args.meta_db)
    try:
        b_run = get_run_id(bconn, args.baseline_run_id)
        m_run = get_run_id(mconn, args.meta_run_id)
        if b_run is None or m_run is None:
            print("Could not find runs in one of the databases.")
            return 2
        baseline = summarize_run(bconn, b_run)
        meta = summarize_run(mconn, m_run)
        iso = isolation_checks(bconn, b_run)
        payload = {
            "timestamp": datetime.now().isoformat(),
            "baseline": baseline,
            "meta": meta,
            "isolation": iso,
            "diffs": {
                "reward_volatility_std": meta["reward_volatility_std"] - baseline["reward_volatility_std"],
                "reflections_count": meta["reflections_count"] - baseline["reflections_count"],
                "trust_drift_per_hour": meta["trust_drift_per_hour"] - baseline["trust_drift_per_hour"],
            }
        }
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)
        print(f"Wrote regression summary to {args.out}")
        print(f"Isolation check (baseline): metacognition={iso['metacognition_rows']}, pred_resolutions={iso['prediction_resolutions_rows']}, isolated={iso['isolated']}")
        return 0
    finally:
        bconn.close()
        mconn.close()


if __name__ == "__main__":
    raise SystemExit(main())
