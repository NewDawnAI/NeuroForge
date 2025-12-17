#!/usr/bin/env python3
import argparse
import json
import os
import sqlite3
import sys


def export_context_peer(db_path: str, out_path: str, peer: str | None, limit: int, since: int | None):
    if not os.path.exists(db_path):
        print(f"[ERROR] DB not found: {db_path}")
        return 1

    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        # Ensure table exists with expected legacy-compatible columns
        cur.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='context_peer_log';")
        row = cur.fetchone()
        if not row:
            print("[ERROR] context_peer_log table missing (expected legacy schema)")
            return 2

        sql = (
            "SELECT run_id, ts_ms, peer, sample, gain, update_ms, window, label "
            "FROM context_peer_log WHERE 1=1"
        )
        params: list[object] = []
        if peer:
            sql += " AND peer = ?"
            params.append(peer)
        if since is not None:
            sql += " AND ts_ms >= ?"
            params.append(since)
        sql += " ORDER BY ts_ms ASC LIMIT ?"  # chronological
        params.append(limit)

        cur.execute(sql, params)
        rows = cur.fetchall()
        series = [
            {
                "run_id": r[0],
                "ts_ms": r[1],
                "peer": r[2],
                "sample": r[3],
                "gain": r[4],
                "update_ms": r[5],
                "window": r[6],
                "label": r[7] if r[7] is not None else "",
            }
            for r in rows
        ]

        payload = {"context_peer": {"series": series, "count": len(series)}}
        os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)
        print(f"[OK] Exported {len(series)} rows to {out_path}")
        return 0
    finally:
        conn.close()


def main():
    ap = argparse.ArgumentParser(description="Export context_peer_log using legacy-compatible schema")
    ap.add_argument("--db", required=True, help="Path to MemoryDB SQLite file")
    ap.add_argument("--out", required=True, help="Output JSON path")
    ap.add_argument("--peer", default=None, help="Filter by peer name")
    ap.add_argument("--limit", type=int, default=100, help="Max rows to export")
    ap.add_argument("--since", type=int, default=None, help="Filter by minimum ts_ms")
    args = ap.parse_args()
    sys.exit(export_context_peer(args.db, args.out, args.peer, args.limit, args.since))


if __name__ == "__main__":
    main()

