import sqlite3
import sys

def main():
    if len(sys.argv) < 2:
        print("Usage: python scripts/list_tables.py <db_path>")
        sys.exit(1)
    db_path = sys.argv[1]
    conn = sqlite3.connect(db_path)
    try:
        rows = conn.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;").fetchall()
        print("Tables:")
        for (name,) in rows:
            print(f"- {name}")
    finally:
        conn.close()

if __name__ == "__main__":
    main()
