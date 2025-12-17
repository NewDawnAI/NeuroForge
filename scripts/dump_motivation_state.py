#!/usr/bin/env python3
import argparse
import json
import os
import sqlite3
from typing import List, Dict, Any

DEFAULT_DB = "phasec_mem.db"
DEFAULT_OUT = os.path.join("pages", "dashboard", "motivation_state.json")

def dump_motivation_state(db_path: str) -> List[Dict[str, Any]]:
    if not os.path.exists(db_path):
        raise FileNotFoundError(f"Database not found: {db_path}")
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    try:
        cur = conn.cursor()
        # Inspect available columns to be robust across schema versions
        cur.execute("PRAGMA table_info(motivation_state)")
        cols = {row[1] for row in cur.fetchall()}  # set of column names
        sel_cols = [c for c in ["ts_ms", "coherence", "avg_goal_satisfaction", "note", "run_id"] if c in cols]
        if not sel_cols:
            return []
        cur.execute(f"SELECT {', '.join(sel_cols)} FROM motivation_state ORDER BY ts_ms ASC")
        rows = cur.fetchall()
        result = []
        for r in rows:
            # Access with defaults if missing
            ts_ms = r["ts_ms"] if "ts_ms" in sel_cols else None
            coherence = float(r["coherence"]) if "coherence" in sel_cols and r["coherence"] is not None else 0.0
            avg_sat = float(r["avg_goal_satisfaction"]) if "avg_goal_satisfaction" in sel_cols and r["avg_goal_satisfaction"] is not None else 0.0
            note = r["note"] if "note" in sel_cols else ""
            run_id = r["run_id"] if "run_id" in sel_cols else None
            result.append({
                "ts_ms": ts_ms,
                "coherence": coherence,
                "avg_goal_satisfaction": avg_sat,
                "note": note or "",
                "run_id": run_id,
            })
        return result
    finally:
        conn.close()


def main():
    ap = argparse.ArgumentParser(description="Dump motivation_state time series to JSON")
    ap.add_argument("--db", type=str, default=DEFAULT_DB, help="Path to sqlite DB (default: phasec_mem.db)")
    ap.add_argument("--out", type=str, default=DEFAULT_OUT, help="Output JSON file path")
    args = ap.parse_args()

    series = dump_motivation_state(args.db)
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump({"series": series}, f)
    print(f"Wrote {len(series)} points to {args.out}")

if __name__ == "__main__":
    main()