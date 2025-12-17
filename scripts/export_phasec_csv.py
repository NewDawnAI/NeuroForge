#!/usr/bin/env python3
import argparse
import csv
import os
import sqlite3
from typing import List, Tuple

TABLES_WITH_RUN = {
    'learning_stats': 'run_id',
    'reward_log': 'run_id',
    'self_model': 'run_id',
    'hippocampal_snapshots': 'run_id',
    'substrate_states': 'run_id',
    'experiences': 'run_id',
    'episodes': 'run_id',
}

JOINED_TABLES = {
    # table_name: (join_sql, where_on_run)
    'episode_stats': (
        'episode_stats es JOIN episodes e ON es.episode_id = e.id',
        'e.run_id'
    ),
    'episode_experiences': (
        'episode_experiences ee JOIN episodes e ON ee.episode_id = e.id',
        'e.run_id'
    ),
}

SKIP_TABLES = set(['sqlite_sequence'])


def get_tables(cur) -> List[str]:
    return [r[0] for r in cur.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name").fetchall()]


def get_columns(cur, table: str) -> List[Tuple[int, str]]:
    # returns list of (cid, name)
    return [(r[0], r[1]) for r in cur.execute(f"PRAGMA table_info({table})").fetchall()]


def export_table(cur, table: str, out_dir: str, run_id: int) -> int:
    # Determine SQL, columns, and write CSV
    if table in SKIP_TABLES:
        return 0

    # Build SELECT
    if table in JOINED_TABLES:
        join_sql, where_col = JOINED_TABLES[table]
        cols = [r[1] for r in cur.execute(f"PRAGMA table_info({table.split()[0]})").fetchall()]  # base table columns
        # For joined tables, select all columns from the primary table plus joined IDs for context
        sql = f"SELECT * FROM {join_sql} WHERE {where_col} = ?"
        rows = cur.execute(sql, (run_id,)).fetchall()
        # Column names from cursor description
        headers = [d[0] for d in cur.description]
    else:
        cols = [r[1] for r in cur.execute(f"PRAGMA table_info({table})").fetchall()]
        if table in TABLES_WITH_RUN:
            sql = f"SELECT * FROM {table} WHERE {TABLES_WITH_RUN[table]} = ?"
            rows = cur.execute(sql, (run_id,)).fetchall()
        else:
            sql = f"SELECT * FROM {table}"
            rows = cur.execute(sql).fetchall()
        headers = [d[0] for d in cur.description]

    out_path = os.path.join(out_dir, f"{table}_run_{run_id}.csv")
    with open(out_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(headers)
        for row in rows:
            writer.writerow([row[i] for i in range(len(headers))])
    return len(rows)


def main():
    ap = argparse.ArgumentParser(description='Export Phase C/NeuroForge telemetry tables to CSV for a given run_id.')
    ap.add_argument('db', help='Path to SQLite DB file')
    ap.add_argument('--out-dir', required=True, help='Directory to write CSV files')
    ap.add_argument('--run-id', type=int, help='Run ID to export; defaults to latest run in DB if not provided')
    args = ap.parse_args()

    db_path = os.path.abspath(args.db)
    out_dir = os.path.abspath(args.out_dir)
    os.makedirs(out_dir, exist_ok=True)

    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()

    # Determine run_id if missing
    run_id = args.run_id
    if run_id is None:
        row = cur.execute('SELECT MAX(id) FROM runs').fetchone()
        run_id = row[0] if row and row[0] is not None else 1
    print(f"DB: {db_path}\nExporting run_id={run_id} to {out_dir}")

    # Enumerate tables and export
    tables = get_tables(cur)
    total_exported = 0
    for t in tables:
        try:
            n = export_table(cur, t, out_dir, run_id)
            print(f"Exported {t}: {n} rows")
            total_exported += n
        except Exception as e:
            print(f"Error exporting {t}: {e}")

    print(f"Done. Total rows exported: {total_exported}")
    conn.close()


if __name__ == '__main__':
    main()