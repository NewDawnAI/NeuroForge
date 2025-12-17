#!/usr/bin/env python3
import argparse
import json
import os
import sqlite3
from typing import Any, Dict, List


def parse_args():
    p = argparse.ArgumentParser(description="Dump Phase 8 goal hierarchy from SQLite to JSON")
    p.add_argument("--db", default="phasec_mem.db", help="Path to SQLite DB (default: phasec_mem.db)")
    p.add_argument("--out", default=os.path.join("pages", "tags", "runner", "goal_graph.json"),
                   help="Output JSON path (default: pages/tags/runner/goal_graph.json)")
    p.add_argument("--run-id", type=int, default=None, help="Run id to export (default: max(run_id) in goal_nodes)")
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


def load_run_id(conn: sqlite3.Connection, run_id_arg: int = None) -> int:
    if run_id_arg is not None:
        return run_id_arg
    if not has_table(conn, 'goal_nodes'):
        return 0
    try:
        cur = conn.execute("SELECT MAX(run_id) FROM goal_nodes")
        max_id = cur.fetchone()[0]
        return max_id if max_id is not None else 0
    except (sqlite3.OperationalError, IndexError):
        return 0

def fetch_nodes(conn: sqlite3.Connection, run_id: int) -> List[Dict[str, Any]]:
    if not has_table(conn, 'goal_nodes'): return []
    cur = conn.execute("SELECT goal_id, description, priority, stability FROM goal_nodes WHERE run_id=?", (run_id,))
    return [{'id': r[0], 'description': r[1], 'priority': r[2], 'stability': r[3]} for r in cur.fetchall()]


def fetch_edges(conn: sqlite3.Connection, run_id: int) -> List[Dict[str, Any]]:
    if not has_table(conn, 'goal_edges'): return []
    # Note: goal_edges table doesn't have run_id, so we select all for now.
    # This might need refinement if goal_ids are not unique across runs.
    cur = conn.execute("SELECT goal_id, subgoal_id, weight FROM goal_edges")
    return [{'source': r[0], 'target': r[1], 'weight': r[2]} for r in cur.fetchall()]


def main():
    args = parse_args()
    if not os.path.exists(args.db):
        raise SystemExit(f"DB not found: {args.db}")
    conn = sqlite3.connect(args.db)
    try:
        run_id = load_run_id(conn, args.run_id)
        nodes = fetch_nodes(conn, run_id)
        edges = fetch_edges(conn, run_id)
        if run_id == 0 and not nodes:
            print("No goal_nodes table or rows found; emitting empty graph.")
        
        payload = {"nodes": nodes, "edges": edges, "run_id": run_id}
        ensure_dir(args.out)
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)
        print(f"Wrote goal graph: {args.out} (nodes={len(nodes)}, edges={len(edges)}, run_id={run_id})")
    finally:
        conn.close()


if __name__ == "__main__":
    main()