import sqlite3
import sys
import os
from collections import defaultdict

def main():
    db = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.getcwd(), "phasec_mem.db")
    if not os.path.exists(db):
        print("DB_NOT_FOUND:", db)
        return 1

    con = sqlite3.connect(db)
    cur = con.cursor()

    # Fetch list of tables
    tables = [t[0] for t in cur.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()]
    print("TABLES:", tables)

    runs = []
    if "runs" in tables:
        runs = cur.execute("SELECT id FROM runs ORDER BY id DESC LIMIT 5").fetchall()
        print("RUNS_LAST5_IDS:", runs)
    latest_run_id = runs[0][0] if runs else None

    metric_cols = {
        "avg_coherence",
        "assemblies",
        "hebbian_updates",
        "stdp_updates",
        "total_updates",
        "avg_energy",
        "metabolic_hazard",
        "step",
    }

    # Optimization: Fetch all columns for all tables in one go using pragma_table_info join
    table_columns = defaultdict(list)
    try:
        # Use a join to get columns for all tables at once
        # Ordering by cid ensures correct column order
        rows = cur.execute("""
            SELECT m.name, p.name
            FROM sqlite_master m
            JOIN pragma_table_info(m.name) p
            WHERE m.type='table'
            ORDER BY m.name, p.cid
        """).fetchall()

        for t_name, c_name in rows:
            table_columns[t_name].append(c_name)

    except Exception as e:
        # Fallback for older SQLite versions that might not support the join or if it fails
        print(f"Bulk schema fetch failed, falling back: {e}")
        table_columns = {}

    found = []
    for t in tables:
        cols = table_columns.get(t)
        if cols is None:
            # Fallback or if table had no columns in the join
            cols = [c[1] for c in cur.execute(f"PRAGMA table_info({t})").fetchall()]

        if any((c in metric_cols) or c.endswith("_updates") or c.endswith("_assemblies") for c in cols):
            found.append((t, cols))
    print("METRIC_TABLES:", found)

    candidate = None
    for t, cols in found:
        if "step" in cols:
            candidate = t
            break
    candidate = candidate or (found[0][0] if found else None)
    print("CANDIDATE_TABLE:", candidate)

    if candidate:
        cols = [c[1] for c in cur.execute(f"PRAGMA table_info({candidate})").fetchall()]
        select_cols = [c for c in cols if (c in metric_cols) or c.endswith("_updates") or c.endswith("_assemblies")]
        where = ""
        params = ()
        if (latest_run_id is not None) and ("run_id" in cols):
            where = " WHERE run_id=?"
            params = (latest_run_id,)
        q = "SELECT " + ", ".join(select_cols) + " FROM " + candidate + where + " ORDER BY rowid DESC LIMIT 20"
        try:
            rows = cur.execute(q, params).fetchall()
            print("LAST_METRICS_ROWS:", rows)
        except Exception as e:
            print("QUERY_ERROR:", e)
            print("QUERY:", q)

    for t in ("learning_stats", "substrate_states"):
        if t in tables:
            cols = [c[1] for c in cur.execute(f"PRAGMA table_info({t})").fetchall()]
            where = ""
            params = ()
            if (latest_run_id is not None) and ("run_id" in cols):
                where = " WHERE run_id=?"
                params = (latest_run_id,)
            q = "SELECT * FROM " + t + where + " ORDER BY rowid DESC LIMIT 5"
            try:
                rows = cur.execute(q, params).fetchall()
                print(t + "_COLS:", cols)
                print(t + "_LAST5:", rows)
            except Exception as e:
                print(t + "_QUERY_ERROR:", e)
                print("QUERY:", q)

    for t in ("parameter_history", "episode_stats", "reward_log"):
        if t in tables:
            cols = [c[1] for c in cur.execute(f"PRAGMA table_info({t})").fetchall()]
            where = ""
            params = ()
            if (latest_run_id is not None) and ("run_id" in cols):
                where = " WHERE run_id=?"
                params = (latest_run_id,)
            q = "SELECT * FROM " + t + where + " ORDER BY rowid DESC LIMIT 5"
            try:
                rows = cur.execute(q, params).fetchall()
                print(t + "_COLS:", cols)
                print(t + "_LAST5:", rows)
            except Exception as e:
                print(t + "_QUERY_ERROR:", e)
                print("QUERY:", q)
    con.close()
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
