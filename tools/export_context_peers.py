#!/usr/bin/env python3
import sqlite3, json, argparse, os, time, sys

def table_has_column(cur, table, col):
    try:
        cur.execute(f"PRAGMA table_info({table})")
        return any((row[1] == col) for row in cur.fetchall())
    except Exception:
        return False

def main():
    p = argparse.ArgumentParser(description="Export context peer streams from MemoryDB")
    p.add_argument("--db", required=True, help="Path to sqlite database")
    p.add_argument("--out", default="web/context_peer_export.json", help="Output JSON path")
    p.add_argument("--limit", type=int, default=10000, help="Max rows to export")
    p.add_argument("--since", type=int, default=None, help="Only export rows with ts_ms greater than this (delta mode)")
    args = p.parse_args()

    if not os.path.exists(args.db):
        print(f"ERROR: Database '{args.db}' not found", file=sys.stderr)
        sys.exit(1)

    conn = sqlite3.connect(args.db)
    cur = conn.cursor()

    # Detect Phase 17b columns dynamically
    has_mode = table_has_column(cur, 'context_peer_log', 'mode')
    has_lambda = table_has_column(cur, 'context_peer_log', 'lambda')
    has_kappa = table_has_column(cur, 'context_peer_log', 'kappa')
    # Legacy optional column retained for backward compatibility
    has_weight = table_has_column(cur, 'context_peer_log', 'weight')

    cols = "ts_ms, peer, label, sample, gain, update_ms, window"
    if has_mode: cols += ", mode"
    if has_lambda: cols += ", lambda"
    if has_kappa: cols += ", kappa"
    if has_weight: cols += ", weight"
    base = f"SELECT {cols} FROM context_peer_log"
    if args.since is not None:
        base += " WHERE ts_ms > ?"
    base += " ORDER BY ts_ms DESC LIMIT ?"
    query = base

    rows = []
    try:
        if args.since is not None:
            cur.execute(query, (args.since, args.limit,))
        else:
            cur.execute(query, (args.limit,))
        rows = cur.fetchall()
    except sqlite3.Error as e:
        # If table doesn't exist yet, emit an empty export so dashboards can load
        if 'no such table' in str(e).lower():
            rows = []
            has_weight = False
            print("WARN: context_peer_log table not found; emitting empty export", file=sys.stderr)
        else:
            print(f"ERROR: Failed to read context_peer_log: {e}", file=sys.stderr)
            sys.exit(2)

    peers = {}
    for row in rows:
        # Unpack dynamically based on detected columns order
        idx = 0
        ts = row[idx]; idx += 1
        peer = row[idx]; idx += 1
        label = row[idx]; idx += 1
        sample = row[idx]; idx += 1
        gain = row[idx]; idx += 1
        upd = row[idx]; idx += 1
        win = row[idx]; idx += 1
        mode = row[idx] if has_mode else None; idx += (1 if has_mode else 0)
        lam = row[idx] if has_lambda else None; idx += (1 if has_lambda else 0)
        kap = row[idx] if has_kappa else None; idx += (1 if has_kappa else 0)
        w = row[idx] if has_weight else None

        cfg = {"gain": gain, "update_ms": upd, "window": win}
        if has_mode: cfg["mode"] = mode
        if has_lambda: cfg["lambda"] = lam
        if has_kappa: cfg["kappa"] = kap
        if has_weight: cfg["weight"] = w

        p = peers.setdefault(peer, {"samples": [], "config": cfg})
        sample_obj = {"ts_ms": ts, "label": label, "value": sample}
        if has_mode and mode is not None: sample_obj["mode"] = mode
        if has_lambda and lam is not None: sample_obj["lambda"] = lam
        if has_kappa and kap is not None: sample_obj["kappa"] = kap
        if has_weight and w is not None: sample_obj["weight"] = w
        p["samples"].append(sample_obj)

    out = {"export_time": int(time.time()*1000), "peers": peers}
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)

    # auto-emit for dashboard alias
    alias = os.path.join("pages", "tags", "runner", "context_peer_stream.json")
    os.makedirs(os.path.dirname(alias), exist_ok=True)
    with open(alias, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)

    print(f"Exported {sum(len(v['samples']) for v in peers.values())} samples across {len(peers)} peers to {args.out} and aliased to {alias}")

if __name__ == "__main__":
    main()
