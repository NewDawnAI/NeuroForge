#!/usr/bin/env python3
"""
Export metacognition rows from MemoryDB to JSON and emit dashboard aliases.

Usage:
  python scripts/export_metacognition_aliases.py --db web/phasec_mem.db --out web/metacognition_export.json [--run_id <id>]

Outputs a JSON file with fields:
{
  "run_id": <int>,
  "metacognition": [
    {
      "ts_ms": ..., "self_trust": ..., "goal_mae": ..., "narrative_rmse": ..., "ece": ...,
      "notes": "...",
      "deltas": {"trust_delta": ..., "coherence_delta": ..., "goal_accuracy_delta": ...},
      "explanation": { ... }  # parsed from self_explanation_json if present
    }
  ],
  "summary": {"count": <int>, "avg_trust": <float>, "avg_goal_acc": <float>}
}
Also writes alias file to pages/tags/runner/metacognition_stream.json for the dashboard.
"""

import argparse
import json
import os
import sqlite3


def latest_run_id(conn: sqlite3.Connection):
    cur = conn.execute("SELECT MAX(id) FROM runs;")
    row = cur.fetchone()
    return int(row[0]) if row and row[0] is not None else None


def fetch_metacognition(conn: sqlite3.Connection, run_id: int):
    cur = conn.execute(
        "SELECT ts_ms, self_trust, narrative_rmse, goal_mae, ece, notes, trust_delta, coherence_delta, goal_accuracy_delta, self_explanation_json "
        "FROM metacognition WHERE run_id = ? ORDER BY ts_ms ASC;",
        (run_id,),
    )
    rows = cur.fetchall()
    series = []
    total_trust = 0.0
    total_goal_acc = 0.0
    count = 0
    for r in rows:
        ts_ms, self_trust, narrative_rmse, goal_mae, ece, notes, trust_delta, coh_delta, goal_acc_delta, expl_json = r
        try:
            explanation = json.loads(expl_json) if expl_json else None
        except Exception:
            explanation = None
        entry = {
            "ts_ms": int(ts_ms),
            "self_trust": float(self_trust) if self_trust is not None else None,
            "goal_mae": float(goal_mae) if goal_mae is not None else None,
            "narrative_rmse": float(narrative_rmse) if narrative_rmse is not None else None,
            "ece": float(ece) if ece is not None else None,
            "notes": notes or "",
            "deltas": {
                "trust_delta": float(trust_delta) if trust_delta is not None else None,
                "coherence_delta": float(coh_delta) if coh_delta is not None else None,
                "goal_accuracy_delta": float(goal_acc_delta) if goal_acc_delta is not None else None,
            },
            "explanation": explanation,
        }
        series.append(entry)
        # accumulate summary stats if values present
        if self_trust is not None:
            total_trust += float(self_trust)
        if goal_mae is not None:
            total_goal_acc += (1.0 - float(goal_mae))
        count += 1
    avg_trust = (total_trust / count) if count else 0.0
    avg_goal_acc = (total_goal_acc / count) if count else 0.0
    return series, {"count": count, "avg_trust": avg_trust, "avg_goal_acc": avg_goal_acc}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", required=True, help="Path to SQLite DB (e.g., web/phasec_mem.db)")
    ap.add_argument("--out", required=True, help="Output JSON path (e.g., web/metacognition_export.json)")
    ap.add_argument("--run_id", type=int, default=None, help="Run id to export; default latest")
    args = ap.parse_args()

    if not os.path.exists(args.db):
        raise SystemExit(f"DB not found: {args.db}")

    conn = sqlite3.connect(args.db)
    try:
        rid = args.run_id or latest_run_id(conn)
        if rid is None:
            raise SystemExit("No runs found in DB")
        series, summary = fetch_metacognition(conn, rid)
        out = {"run_id": rid, "metacognition": series, "summary": summary}
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2)
        print(f"Wrote {args.out} (entries={summary['count']})")
        # Emit dashboard alias
        alias_dir = os.path.join("pages", "tags", "runner")
        os.makedirs(alias_dir, exist_ok=True)
        alias_path = os.path.join(alias_dir, "metacognition_stream.json")
        with open(alias_path, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2)
        print(f"Alias written: {alias_path}")
    finally:
        conn.close()


if __name__ == "__main__":
    main()
