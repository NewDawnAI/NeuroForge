#!/usr/bin/env python3
import argparse
import json
import os
import sqlite3
from typing import Any, Dict, List


def parse_args():
    p = argparse.ArgumentParser(description="Dump Phase 7 reflections from SQLite to JSON")
    p.add_argument("--db", default="phasec_mem.db", help="Path to SQLite DB (default: phasec_mem.db)")
    p.add_argument("--out", default=os.path.join("pages", "tags", "runner", "reflections.json"),
                   help="Output JSON path (default: pages/tags/runner/reflections.json)")
    p.add_argument("--run-id", type=int, default=None, help="Run id to export (default: max(run_id) in reflections)")
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
    if not has_table(conn, "reflections"):
        return 0
    cur = conn.execute("SELECT COALESCE(MAX(run_id), 0) FROM reflections")
    row = cur.fetchone()
    rid = int(row[0] or 0)
    return rid


def fetch_reflections(conn: sqlite3.Connection, run_id: int) -> List[Dict[str, Any]]:
    if not has_table(conn, "reflections"):
        return []
    cur = conn.execute(
        "SELECT id, ts_ms, title, rationale_json, impact, episode FROM reflections WHERE run_id=? ORDER BY ts_ms ASC",
        (run_id,)
    )
    reflections = []
    for rid, ts_ms, title, rationale_json, impact, episode in cur.fetchall():
        rationale: Any
        try:
            rationale = json.loads(rationale_json) if rationale_json else {}
        except Exception:
            rationale = {"raw": rationale_json}
        reflections.append({
            "id": int(rid),
            "timestamp": int(ts_ms),
            "title": str(title or ""),
            "rationale": rationale,
            "impact": float(impact or 0.0),
            "episode": int(episode) if episode is not None else None,
        })
    return reflections


def main():
    args = parse_args()
    if not os.path.exists(args.db):
        raise SystemExit(f"DB not found: {args.db}")
    conn = sqlite3.connect(args.db)
    try:
        run_id = load_run_id(conn, args.run_id)
        if run_id == 0:
            print("No reflections table or rows found; emitting empty list.")
        reflections = fetch_reflections(conn, run_id)
        payload = {"reflections": reflections, "run_id": run_id}
        ensure_dir(args.out)
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)
        print(f"Wrote reflections: {args.out} (count={len(reflections)}, run_id={run_id})")
    finally:
        conn.close()


if __name__ == "__main__":
    main()