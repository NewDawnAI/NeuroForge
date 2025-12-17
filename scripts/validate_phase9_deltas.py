#!/usr/bin/env python3
import argparse
import sqlite3
import statistics
from typing import Optional, Tuple

def fetch_deltas(conn: sqlite3.Connection, run_id: int):
    cur = conn.cursor()
    cur.execute(
        """
        SELECT trust_delta, coherence_delta, goal_accuracy_delta
        FROM metacognition
        WHERE run_id = ? AND ts_ms IS NOT NULL
        ORDER BY ts_ms ASC
        """,
        (run_id,)
    )
    rows = cur.fetchall()
    def clean(col_idx):
        vals = [r[col_idx] for r in rows if r[col_idx] is not None]
        return vals
    return clean(0), clean(1), clean(2)

def summarize(name: str, vals):
    if not vals:
        return f"{name}: n=0"
    mean = statistics.fmean(vals)
    median = statistics.median(vals)
    pos_frac = sum(1 for v in vals if v is not None and v > 0) / len(vals)
    return f"{name}: n={len(vals)} mean={mean:.4f} median={median:.4f} positive_fraction={pos_frac:.3f}"

def compare_runs(db_path: str, control_id: int, introspective_id: int):
    conn = sqlite3.connect(db_path)
    try:
        c_trust, c_coh, c_goal = fetch_deltas(conn, control_id)
        i_trust, i_coh, i_goal = fetch_deltas(conn, introspective_id)
        print(f"DB: {db_path}")
        print(f"Control run_id={control_id}")
        print("  " + summarize("Δtrust", c_trust))
        print("  " + summarize("Δcoherence", c_coh))
        print("  " + summarize("Δgoal_accuracy", c_goal))
        print(f"Introspective run_id={introspective_id}")
        print("  " + summarize("Δtrust", i_trust))
        print("  " + summarize("Δcoherence", i_coh))
        print("  " + summarize("Δgoal_accuracy", i_goal))
        # Simple effect sizes (difference of means)
        def diff_mean(a, b):
            if not a or not b:
                return None
            return statistics.fmean(b) - statistics.fmean(a)
        print("Effects (introspective - control):")
        dm_trust = diff_mean(c_trust, i_trust)
        dm_coh = diff_mean(c_coh, i_coh)
        dm_goal = diff_mean(c_goal, i_goal)
        print(f"  Δtrust mean diff: {dm_trust:.4f}" if dm_trust is not None else "  Δtrust mean diff: n/a")
        print(f"  Δcoherence mean diff: {dm_coh:.4f}" if dm_coh is not None else "  Δcoherence mean diff: n/a")
        print(f"  Δgoal_accuracy mean diff: {dm_goal:.4f}" if dm_goal is not None else "  Δgoal_accuracy mean diff: n/a")
    finally:
        conn.close()

def list_recent_runs(db_path: str, limit: int = 10):
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        cur.execute("SELECT id, started_ms, metadata_json FROM runs ORDER BY id DESC LIMIT ?", (limit,))
        rows = cur.fetchall()
        print("Recent runs:")
        for r in rows:
            rid, started_ms, meta = r
            print(f"  id={rid} started_ms={started_ms} meta={meta[:80] if meta else ''}")
    finally:
        conn.close()

def main():
    parser = argparse.ArgumentParser(description="Compare Phase 9 delta metrics between two runs")
    parser.add_argument("--db", default="c:/Users/ashis/Desktop/NeuroForge/phasec_mem.db", help="Path to SQLite DB")
    parser.add_argument("--control", type=int, help="Control run_id")
    parser.add_argument("--introspective", type=int, help="Introspective run_id")
    parser.add_argument("--list", action="store_true", help="List recent runs and exit")
    args = parser.parse_args()

    if args.list:
        list_recent_runs(args.db)
        return
    if args.control is None or args.introspective is None:
        print("Please pass --control and --introspective run IDs. Use --list to inspect.")
        return

    compare_runs(args.db, args.control, args.introspective)

if __name__ == "__main__":
    main()
