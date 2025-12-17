#!/usr/bin/env python3
import argparse
import sqlite3
import os
from pathlib import Path

SCHEMA_SQL = """
PRAGMA journal_mode=WAL;
CREATE TABLE IF NOT EXISTS goal_nodes (
  run_id INTEGER NOT NULL,
  goal_id INTEGER PRIMARY KEY,
  description TEXT,
  priority REAL,
  stability REAL,
  origin_reflection_id INT
);
CREATE TABLE IF NOT EXISTS goal_edges (
  goal_id INT,
  subgoal_id INT,
  weight REAL
);
CREATE TABLE IF NOT EXISTS motivation_state (
  run_id INTEGER NOT NULL,
  ts_ms INTEGER NOT NULL,
  motivation REAL,
  coherence REAL,
  notes TEXT
);
"""


def parse_args():
    p = argparse.ArgumentParser(description="Create Phase 8 tables if missing")
    p.add_argument("--db", default="phasec_mem.db", help="Path to SQLite DB (default: phasec_mem.db)")
    return p.parse_args()


def main():
    args = parse_args()
    db = Path(args.db)
    if not db.exists():
        raise SystemExit(f"DB not found: {db}")
    conn = sqlite3.connect(str(db))
    try:
        conn.executescript(SCHEMA_SQL)
        conn.commit()
        print("Phase 8 schema ensured: goal_nodes, goal_edges, motivation_state")
    finally:
        conn.close()


if __name__ == "__main__":
    main()