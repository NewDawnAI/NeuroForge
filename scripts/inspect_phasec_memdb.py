import argparse
import sqlite3
import os
import sys


def connect(path: str) -> sqlite3.Connection:
    if not os.path.isabs(path):
        path = os.path.abspath(path)
    print(f"Inspecting DB at: {path}")
    conn = sqlite3.connect(path)
    conn.row_factory = sqlite3.Row
    return conn


def list_tables(conn: sqlite3.Connection):
    cur = conn.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;")
    return [r[0] for r in cur.fetchall()]


def count_table(conn: sqlite3.Connection, name: str):
    try:
        cur = conn.execute(f"SELECT COUNT(*) AS c FROM {name};")
        return cur.fetchone()[0]
    except sqlite3.Error as e:
        print(f"[WARN] count failed for {name}: {e}")
        return None


def preview_recent(conn: sqlite3.Connection, name: str, order_by: str = "id", limit: int = 5):
    try:
        cur = conn.execute(f"SELECT * FROM {name} ORDER BY {order_by} DESC LIMIT {limit};")
        rows = cur.fetchall()
        return [dict(row) for row in rows]
    except sqlite3.Error as e:
        print(f"[WARN] preview failed for {name}: {e}")
        return []

# Added: list columns helper for debugging schema alignment

def list_columns(conn: sqlite3.Connection, name: str):
    try:
        cur = conn.execute(f"PRAGMA table_info({name});")
        return [(row[1], row[2]) for row in cur.fetchall()]  # (name, type)
    except sqlite3.Error as e:
        print(f"[WARN] table_info failed for {name}: {e}")
        return []


def main():
    ap = argparse.ArgumentParser(description="Inspect Phase C MemoryDB SQLite")
    ap.add_argument("--db", default="phasec_mem.db", help="Path to SQLite DB")
    args = ap.parse_args()

    if not os.path.exists(args.db):
        print(f"[ERROR] DB not found: {args.db}")
        sys.exit(1)

    conn = connect(args.db)
    try:
        tables = list_tables(conn)
        print("Tables:")
        for t in tables:
            print(f"  - {t}")

        key_tables = [
            "runs",
            "learning_stats",
            "reward_log",
            "episodes",
            "experiences",
            "episode_experiences",
            "episode_stats",
        ]
        print("\nRow counts:")
        for t in key_tables:
            if t in tables:
                c = count_table(conn, t)
                print(f"  - {t}: {c}")
            else:
                print(f"  - {t}: [missing]")

        # Added: dump learning_stats schema columns to diagnose insertion mismatch
        if "learning_stats" in tables:
            cols = list_columns(conn, "learning_stats")
            print("\nlearning_stats columns:")
            for name, ty in cols:
                print(f"  - {name} ({ty})")

        if "learning_stats" in tables:
            print("\nRecent learning_stats (latest 5 by ts_ms):")
            for row in preview_recent(conn, "learning_stats", order_by="ts_ms", limit=5):
                print(row)
        else:
            print("\nRecent learning_stats: [table missing]")

        if "reward_log" in tables:
            print("\nRecent reward_log (latest 5 by id):")
            for row in preview_recent(conn, "reward_log", order_by="id", limit=5):
                print(row)
        else:
            print("\nRecent reward_log: [table missing]")
    finally:
        conn.close()


if __name__ == "__main__":
    main()