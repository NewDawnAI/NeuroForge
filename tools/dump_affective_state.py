#!/usr/bin/env python3
import argparse
import json
import os
import sqlite3
from typing import Any, Dict, List


def parse_args():
    p = argparse.ArgumentParser(description="Dump Phase 7 affective_state timeseries from SQLite to JSON")
    p.add_argument("--db", default="phasec_mem.db", help="Path to SQLite DB (default: phasec_mem.db)")
    p.add_argument("--out", default=os.path.join("pages", "tags", "runner", "affective_state.json"),
                   help="Output JSON path (default: pages/tags/runner/affective_state.json)")
    p.add_argument("--run-id", type=int, default=None, help="Run id to export (default: max(run_id) in affective_state)")
    p.add_argument("--limit", type=int, default=10000, help="Max rows to export (default: 10000)")
    return p.parse_args()


def ensure_dir(path: str):
    d = os.path.dirname(path)
    if d and not os.path.exists(d):
        os.makedirs(d, exist_ok=True)


def has_table(conn: sqlite3.Connection, name: str) -> bool:
    try:
        cur = conn.execute("SELECT name FROM sqlite_master WHERE type='table' AND name=?", (name,))
        return cur.fetchone() is not None
    except sqlite3.Error:
        return False


def load_run_id(conn: sqlite3.Connection, explicit: int | None) -> int:
    if explicit is not None:
        return explicit
    if not has_table(conn, "affective_state"):
        return 0
    cur = conn.execute("SELECT COALESCE(MAX(run_id), 0) FROM affective_state")
    row = cur.fetchone()
    rid = int(row[0] or 0)
    return rid


def fetch_affective(conn: sqlite3.Connection, run_id: int, limit: int) -> List[Dict[str, Any]]:
    if not has_table(conn, "affective_state"):
        return []
    cur = conn.execute(
        "SELECT id, ts_ms, valence, arousal, focus, notes FROM affective_state WHERE run_id=? ORDER BY ts_ms ASC LIMIT ?",
        (run_id, limit)
    )
    rows = []
    for rid, ts_ms, valence, arousal, focus, notes in cur.fetchall():
        rows.append({
            "id": int(rid),
            "timestamp": int(ts_ms),
            "valence": float(valence or 0.0),
            "arousal": float(arousal or 0.0),
            "focus": float(focus or 0.0),
            "notes": str(notes or "")
        })
    return rows


def main():
    args = parse_args()
    if not os.path.exists(args.db):
        raise SystemExit(f"DB not found: {args.db}")
    conn = sqlite3.connect(args.db)
    try:
        run_id = load_run_id(conn, args.run_id)
        if run_id == 0:
            print("No affective_state table or rows found; emitting empty timeseries.")
        rows = fetch_affective(conn, run_id, args.limit)
        payload = {"affective_state": rows, "run_id": run_id}
        ensure_dir(args.out)
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)
        print(f"Wrote affective_state: {args.out} (count={len(rows)}, run_id={run_id})")
    finally:
        conn.close()


if __name__ == "__main__":
    main()