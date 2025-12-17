#!/usr/bin/env python3
import argparse
import json
import os
import sqlite3
from typing import Any, Dict, List

SCHEMA = {
    "nodes": [],
    "edges": []
}


def parse_args():
    p = argparse.ArgumentParser(description="Dump Phase 7 intent graph from SQLite to JSON")
    p.add_argument("--db", default="phasec_mem.db", help="Path to SQLite DB (default: phasec_mem.db)")
    p.add_argument("--out", default=os.path.join("pages", "tags", "runner", "intent_graph.json"),
                   help="Output JSON path (default: pages/tags/runner/intent_graph.json)")
    p.add_argument("--run-id", type=int, default=None, help="Run id to export (default: max(run_id) in intent_nodes)")
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
    if not has_table(conn, "intent_nodes"):
        return 0
    cur = conn.execute("SELECT COALESCE(MAX(run_id), 0) FROM intent_nodes")
    row = cur.fetchone()
    rid = int(row[0] or 0)
    return rid


def fetch_nodes(conn: sqlite3.Connection, run_id: int) -> List[Dict[str, Any]]:
    if not has_table(conn, "intent_nodes"):
        return []
    cur = conn.execute(
        "SELECT id, node_type, confidence, state_json FROM intent_nodes WHERE run_id=? ORDER BY id ASC",
        (run_id,)
    )
    nodes = []
    for nid, ntype, conf, state_json in cur.fetchall():
        state: Any
        try:
            state = json.loads(state_json) if state_json else {}
        except Exception:
            state = {"raw": state_json}
        nodes.append({
            "id": int(nid),
            "type": str(ntype or "unknown"),
            "confidence": float(conf or 0.0),
            "state": state,
        })
    return nodes


def fetch_edges(conn: sqlite3.Connection, run_id: int) -> List[Dict[str, Any]]:
    if not has_table(conn, "intent_edges"):
        return []
    cur = conn.execute(
        "SELECT from_node_id, to_node_id, cause, weight FROM intent_edges WHERE run_id=? ORDER BY id ASC",
        (run_id,)
    )
    edges = []
    for src, dst, cause, weight in cur.fetchall():
        edges.append({
            "source": int(src),
            "target": int(dst),
            "cause": str(cause or ""),
            "weight": float(weight or 0.0),
        })
    return edges


def main():
    args = parse_args()
    if not os.path.exists(args.db):
        raise SystemExit(f"DB not found: {args.db}")
    conn = sqlite3.connect(args.db)
    try:
        run_id = load_run_id(conn, args.run_id)
        if run_id == 0:
            print("No intent_nodes table or rows found; emitting empty graph.")
        nodes = fetch_nodes(conn, run_id)
        edges = fetch_edges(conn, run_id)
        payload = {"nodes": nodes, "edges": edges, "run_id": run_id}
        ensure_dir(args.out)
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)
        print(f"Wrote intent graph: {args.out} (nodes={len(nodes)}, edges={len(edges)}, run_id={run_id})")
    finally:
        conn.close()


if __name__ == "__main__":
    main()