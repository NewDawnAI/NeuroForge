#!/usr/bin/env python3
"""
Interactive SQLite REPL for NeuroForge MemoryDB.

Usage:
  python tools/db_repl.py --db build/phasec_mem.db
Commands inside REPL:
  :tables                 List tables
  :schema <table>         Show schema
  :runs                   Show runs (id, ts)
  :episodes <run_id>      List episodes for run_id
  :rewards <run_id> [N]   Tail N recent rewards (default 10)
  :substrate <run_id> [N] Tail N substrate_states rows
  Any SQL query           Execute and print top 50 rows
  :quit                   Exit
"""
import argparse
import sqlite3
import sys
from pathlib import Path


def connect(db_path: Path):
    conn = sqlite3.connect(str(db_path))
    conn.row_factory = sqlite3.Row
    return conn


def list_tables(conn):
    cur = conn.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")
    return [r[0] for r in cur.fetchall()]


def print_rows(rows, limit=50):
    if not rows:
        print("(no rows)")
        return
    cols = rows[0].keys()
    print('| ' + ' | '.join(cols) + ' |')
    for i, r in enumerate(rows[:limit]):
        print('| ' + ' | '.join(str(r[c]) for c in cols) + ' |')
    if len(rows) > limit:
        print(f"... ({len(rows)-limit} more)")


def main():
    ap = argparse.ArgumentParser(description='NeuroForge MemoryDB REPL')
    ap.add_argument('--db', default=str(Path('build') / 'phasec_mem.db'))
    args = ap.parse_args()
    db_path = Path(args.db)
    if not db_path.exists():
        print(f"ERROR: DB not found: {db_path}")
        sys.exit(1)
    conn = connect(db_path)
    print(f"Connected to {db_path}")
    print("Type :quit to exit. Use :tables, :schema <table>, :runs, :episodes <run_id>, :rewards <run_id> [N], :substrate <run_id> [N].")
    while True:
        try:
            line = input('nf-sql> ').strip()
        except EOFError:
            break
        if not line:
            continue
        if line in (':quit', ':exit'):
            break
        if line == ':tables':
            print('\n'.join(list_tables(conn)))
            continue
        if line.startswith(':schema '):
            t = line.split(' ', 1)[1].strip()
            cur = conn.execute(f"PRAGMA table_info({t})")
            rows = cur.fetchall()
            print_rows(rows)
            continue
        if line == ':runs':
            cur = conn.execute("SELECT id, start_ms, end_ms FROM runs ORDER BY id DESC")
            print_rows(cur.fetchall())
            continue
        if line.startswith(':episodes '):
            parts = line.split()
            run_id = int(parts[1])
            cur = conn.execute("SELECT id, run_id, start_ms, end_ms, reward_sum FROM episodes WHERE run_id=? ORDER BY id DESC", (run_id,))
            print_rows(cur.fetchall())
            continue
        if line.startswith(':rewards '):
            parts = line.split()
            run_id = int(parts[1])
            limit = int(parts[2]) if len(parts) > 2 else 10
            cur = conn.execute("SELECT ts_ms, reward, notes FROM reward_log WHERE run_id=? ORDER BY ts_ms DESC LIMIT ?", (run_id, limit))
            print_rows(cur.fetchall())
            continue
        if line.startswith(':substrate '):
            parts = line.split()
            run_id = int(parts[1])
            limit = int(parts[2]) if len(parts) > 2 else 10
            cur = conn.execute("SELECT step, avg_coherence, assemblies, bindings FROM substrate_states WHERE run_id=? ORDER BY step DESC LIMIT ?", (run_id, limit))
            print_rows(cur.fetchall())
            continue
        # Treat everything else as SQL
        try:
            cur = conn.execute(line)
            rows = cur.fetchall()
            print_rows(rows)
        except Exception as e:
            print(f"SQL error: {e}")
    conn.close()
    print("Disconnected.")


if __name__ == '__main__':
    main()

