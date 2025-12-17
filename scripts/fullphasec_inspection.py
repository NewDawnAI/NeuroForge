import sqlite3
import os
import sys
import pandas as pd

# Configuration
DB_PATH = r"C:\Users\ashis\Desktop\NeuroForge\phasec_mem.db"  # Change this if your file is elsewhere

def inspect_database(db_path):
    if not os.path.exists(db_path):
        print(f"[ERROR] Database file not found at: {db_path}")
        return

    print(f"--- INSPECTING DATABASE: {db_path} ---")
    
    try:
        # Connect to the database
        # uri=True allows for read-only mode if needed, but here we use standard
        conn = sqlite3.connect(db_path)
        conn.row_factory = sqlite3.Row  # Allows accessing columns by name
        cursor = conn.cursor()

        # 1. Attempt WAL Checkpoint (Merge temp data to main file)
        try:
            print("[INFO] Attempting WAL checkpoint to flush pending data...")
            cursor.execute("PRAGMA wal_checkpoint(PASSIVE);")
            print("[INFO] Checkpoint complete.")
        except Exception as e:
            print(f"[WARN] Could not checkpoint WAL: {e}")

        # 2. Get List of All Tables
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;")
        tables = [row[0] for row in cursor.fetchall() if row[0] != 'sqlite_sequence']
        
        print(f"\n[INFO] Found {len(tables)} tables.")

        # 3. Iterate through every table
        for table in tables:
            print(f"\n{'='*60}")
            print(f"TABLE: {table}")
            print(f"{'='*60}")

            # A. Get Row Count
            try:
                count = cursor.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                print(f"Total Rows: {count}")
            except Exception as e:
                print(f"Error counting rows: {e}")
                continue

            # B. Get Schema (Columns)
            print("\n--- Schema ---")
            try:
                # Returns: (cid, name, type, notnull, dflt_value, pk)
                columns_info = cursor.execute(f"PRAGMA table_info({table})").fetchall()
                columns = [col[1] for col in columns_info]
                for col in columns_info:
                    pk_str = " [PK]" if col[5] else ""
                    print(f" - {col[1]} ({col[2]}){pk_str}")
            except Exception as e:
                print(f"Error fetching schema: {e}")

            # C. Sample Data (Latest 3 rows)
            if count > 0:
                print(f"\n--- Latest 3 Rows (Sorted by ID DESC) ---")
                try:
                    # Assumes 'id' exists, otherwise falls back to default sort
                    if 'id' in columns:
                        query = f"SELECT * FROM {table} ORDER BY id DESC LIMIT 3"
                    else:
                        query = f"SELECT * FROM {table} LIMIT 3"
                    
                    # Use pandas for nice formatting
                    df = pd.read_sql_query(query, conn)
                    
                    # Truncate long JSON strings for readability
                    for col in df.columns:
                        if df[col].dtype == object:
                            df[col] = df[col].astype(str).apply(lambda x: (x[:75] + '...') if len(x) > 75 else x)
                    
                    print(df.to_string(index=False))
                except Exception as e:
                    print(f"Error fetching sample: {e}")
            else:
                print("\n(Table is empty)")

    except sqlite3.Error as e:
        print(f"[CRITICAL ERROR] SQLite error: {e}")
    finally:
        if conn:
            conn.close()
        print("\n--- INSPECTION COMPLETE ---")

if __name__ == "__main__":
    # Allow passing path as argument
    target_db = sys.argv[1] if len(sys.argv) > 1 else DB_PATH
    inspect_database(target_db)