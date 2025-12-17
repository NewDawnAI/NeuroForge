import sqlite3
import json
import sys
from pathlib import Path


def list_tables(db_path: str):
    p = Path(db_path)
    if not p.exists():
        print(json.dumps({
            "error": f"Database not found: {db_path}",
            "cwd": str(Path.cwd())
        }, indent=2))
        return 1
    conn = sqlite3.connect(str(p))
    try:
        cur = conn.cursor()
        cur.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")
        tables = [r[0] for r in cur.fetchall()]
        print(json.dumps({"db": str(p), "tables": tables}, indent=2))
        return 0
    finally:
        conn.close()


if __name__ == "__main__":
    db = sys.argv[1] if len(sys.argv) > 1 else "web/phasec_mem.db"
    sys.exit(list_tables(db))

