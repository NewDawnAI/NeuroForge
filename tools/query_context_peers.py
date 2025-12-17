#!/usr/bin/env python3
import sqlite3, argparse, sys, os

def main():
    p = argparse.ArgumentParser(description="Print last N rows from context_peer_log for quick inspection")
    p.add_argument("--db", required=True, help="Path to sqlite database")
    p.add_argument("--limit", type=int, default=20, help="Number of rows to show")
    p.add_argument("--peer", default=None, help="Filter by peer name")
    args = p.parse_args()

    if not os.path.exists(args.db):
        print(f"ERROR: Database '{args.db}' not found", file=sys.stderr)
        sys.exit(1)

    conn = sqlite3.connect(args.db)
    cur = conn.cursor()

    # Detect optional weight column
    try:
        cur.execute("PRAGMA table_info(context_peer_log)")
        cols = [r[1] for r in cur.fetchall()]
    except sqlite3.Error as e:
        print(f"ERROR: Failed to introspect table: {e}", file=sys.stderr)
        sys.exit(2)

    if 'ts_ms' not in cols or 'peer' not in cols:
        print("ERROR: context_peer_log table not found or missing required columns", file=sys.stderr)
        sys.exit(3)

    has_weight = ('weight' in cols)
    select_cols = "ts_ms, peer, label, sample" + (", weight" if has_weight else "")
    where = " WHERE peer = ?" if args.peer else ""
    query = f"SELECT {select_cols} FROM context_peer_log{where} ORDER BY ts_ms DESC LIMIT ?"

    try:
        if args.peer:
            cur.execute(query, (args.peer, args.limit))
        else:
            cur.execute(query, (args.limit,))
        rows = cur.fetchall()
    except sqlite3.Error as e:
        print(f"ERROR: Query failed: {e}", file=sys.stderr)
        sys.exit(4)

    for r in rows:
        ts, peer, label, sample = r[:4]
        w = r[4] if has_weight and len(r) > 4 else None
        wtxt = (f" weight={w}" if w is not None else "")
        print(f"ts_ms={ts} peer={peer} label={label} sample={sample}{wtxt}")

if __name__ == "__main__":
    main()

