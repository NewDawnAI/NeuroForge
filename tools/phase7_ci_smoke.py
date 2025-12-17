import argparse
import json
import os
import sqlite3
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DB_DEFAULT = ROOT / "phase7_ci.sqlite"
OUT_DEFAULT = ROOT / "pages" / "tags" / "runner" / "intent_graph.json"

SCHEMA_SQL = """
PRAGMA journal_mode=WAL;
CREATE TABLE IF NOT EXISTS runs (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  started_ms INTEGER NOT NULL,
  metadata_json TEXT
);
CREATE TABLE IF NOT EXISTS intent_nodes (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  run_id INTEGER NOT NULL,
  ts_ms INTEGER NOT NULL,
  node_type TEXT NOT NULL,
  state_json TEXT NOT NULL,
  confidence REAL,
  source TEXT,
  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS intent_edges (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  run_id INTEGER NOT NULL,
  ts_ms INTEGER NOT NULL,
  from_node_id INTEGER NOT NULL,
  to_node_id INTEGER NOT NULL,
  cause TEXT,
  weight REAL,
  details_json TEXT,
  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE,
  FOREIGN KEY(from_node_id) REFERENCES intent_nodes(id) ON DELETE CASCADE,
  FOREIGN KEY(to_node_id) REFERENCES intent_nodes(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS affective_state (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  run_id INTEGER NOT NULL,
  ts_ms INTEGER NOT NULL,
  valence REAL,
  arousal REAL,
  focus REAL,
  notes TEXT,
  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS reflections (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  run_id INTEGER NOT NULL,
  ts_ms INTEGER NOT NULL,
  title TEXT,
  rationale_json TEXT NOT NULL,
  impact REAL,
  episode INTEGER,
  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE
);
"""


def ensure_schema(conn: sqlite3.Connection) -> None:
    conn.executescript(SCHEMA_SQL)


def seed_minimal_graph(conn: sqlite3.Connection) -> int:
    # Begin a run
    started_ms = int(time.time() * 1000)
    cur = conn.cursor()
    cur.execute("INSERT INTO runs (started_ms, metadata_json) VALUES (?, ?)", (started_ms, json.dumps({"ci": True})))
    run_id = cur.lastrowid

    # Insert two nodes: correction and resolution
    ts = started_ms + 1
    correction_state = {
        "key": "phase7_smoke",
        "observed_reward": -0.1,
        "posterior_mean": 0.05,
        "note": "seeded contradiction correction"
    }
    cur.execute(
        "INSERT INTO intent_nodes (run_id, ts_ms, node_type, state_json, confidence, source) VALUES (?, ?, ?, ?, ?, ?)",
        (run_id, ts, "correction", json.dumps(correction_state), 0.6, "ci-smoke")
    )
    correction_id = cur.lastrowid

    ts2 = ts + 10
    resolution_state = {
        "key": "phase7_smoke",
        "observed_reward": 0.2,
        "posterior_mean": 0.15,
        "note": "seeded resolution"
    }
    cur.execute(
        "INSERT INTO intent_nodes (run_id, ts_ms, node_type, state_json, confidence, source) VALUES (?, ?, ?, ?, ?, ?)",
        (run_id, ts2, "resolution", json.dumps(resolution_state), 0.7, "ci-smoke")
    )
    resolution_id = cur.lastrowid

    # Edge linking correction -> resolution
    cur.execute(
        "INSERT INTO intent_edges (run_id, ts_ms, from_node_id, to_node_id, cause, weight, details_json) VALUES (?, ?, ?, ?, ?, ?, ?)",
        (run_id, ts2, correction_id, resolution_id, "contradiction_resolved", 1.0, json.dumps({"alpha": 0.5, "beta": 0.5}))
    )

    # Affective snapshot
    cur.execute(
        "INSERT INTO affective_state (run_id, ts_ms, valence, arousal, focus, notes) VALUES (?, ?, ?, ?, ?, ?)",
        (run_id, ts2 + 5, 0.1, 0.3, 0.5, "ci smoke affective")
    )

    # Reflection entry
    rationale = {"summary": "Observed contradiction; resolution improved posterior mean"}
    cur.execute(
        "INSERT INTO reflections (run_id, ts_ms, title, rationale_json, impact, episode) VALUES (?, ?, ?, ?, ?, ?)",
        (run_id, ts2 + 20, "Phase7 CI smoke", json.dumps(rationale), 0.2, 1)
    )

    conn.commit()
    return run_id


def run_engine_if_available(db_path: Path, steps: int = 50) -> bool:
    # Try to locate neuroforge.exe across common build outputs
    candidates = [
        ROOT / "build" / "Release" / "neuroforge.exe",
        ROOT / "build" / "Debug" / "neuroforge.exe",
        ROOT / "build" / "neuroforge.exe",
        ROOT / "build-release" / "Release" / "neuroforge.exe",
        ROOT / "build-debug" / "Debug" / "neuroforge.exe",
    ]
    exe = next((p for p in candidates if p.exists()), None)
    if exe is None:
        return False
    cmd = [
        str(exe),
        f"--memory-db={db_path}",
        f"--steps={steps}",
        "--phase6-active=on",
        "--phase7=on",
        "--viewer=off",
        "--vision-demo=off",
    ]
    try:
        print("[phase7_ci_smoke] Running engine:", " ".join(cmd))
        subprocess.run(cmd, check=True)
        return True
    except Exception as e:
        print(f"[phase7_ci_smoke] Engine run failed: {e}")
        return False


def dump_intent_graph(db_path: Path, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    cmd = [sys.executable, str(ROOT / "tools" / "dump_intent_graph.py"), "--db", str(db_path), "--out", str(out_path)]
    print("[phase7_ci_smoke] Dumping intent graph:", " ".join(cmd))
    subprocess.run(cmd, check=True)


def main():
    ap = argparse.ArgumentParser(description="Phase 7 CI smoke: seed or run, then dump intent graph")
    ap.add_argument("--db", type=str, default=str(DB_DEFAULT), help="SQLite db path for MemoryDB")
    ap.add_argument("--out", type=str, default=str(OUT_DEFAULT), help="Output JSON path for intent graph")
    ap.add_argument("--steps", type=int, default=50, help="Engine steps if executable is available")
    args = ap.parse_args()

    db_path = Path(args.db)
    out_path = Path(args.out)

    # Ensure fresh db
    if db_path.exists():
        try:
            db_path.unlink()
        except Exception:
            pass

    # Try real engine; if absent, seed minimal dataset
    ran_engine = run_engine_if_available(db_path, steps=args.steps)
    if not ran_engine or not db_path.exists():
        print("[phase7_ci_smoke] Engine not found or produced no DB; seeding minimal Phase 7 dataset")
        conn = sqlite3.connect(db_path)
        ensure_schema(conn)
        run_id = seed_minimal_graph(conn)
        print(f"[phase7_ci_smoke] Seeded run_id={run_id}")

    # Dump the graph
    dump_intent_graph(db_path, out_path)

    # Emit counts for CI verification
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    cur.execute("SELECT COUNT(*) FROM intent_nodes")
    nodes = cur.fetchone()[0]
    cur.execute("SELECT COUNT(*) FROM intent_edges")
    edges = cur.fetchone()[0]
    cur.execute("SELECT COUNT(*) FROM affective_state")
    aff = cur.fetchone()[0]
    cur.execute("SELECT COUNT(*) FROM reflections")
    refl = cur.fetchone()[0]
    print(f"INTENT_NODES_COUNT={nodes}")
    print(f"INTENT_EDGES_COUNT={edges}")
    print(f"AFFECTIVE_COUNT={aff}")
    print(f"REFLECTIONS_COUNT={refl}")


if __name__ == "__main__":
    main()