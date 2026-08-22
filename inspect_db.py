import sqlite3
import os

db_path = 'phasec_mem.db'

if not os.path.exists(db_path):
    print(f"Error: {db_path} not found.")
    exit(1)

try:
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    # Get tables
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
    tables = cursor.fetchall()

    print(f"Found {len(tables)} tables.")

    for table in tables:
        table_name = table[0]
        print(f"\n--- Table: {table_name} ---")
        
        # Schema
        cursor.execute(f"SELECT sql FROM sqlite_master WHERE type='table' AND name='{table_name}';")
        schema = cursor.fetchone()[0]
        print("Schema:")
        print(schema)
        
        # Count
        cursor.execute(f"SELECT COUNT(*) FROM {table_name};")
        count = cursor.fetchone()[0]
        print(f"Row count: {count}")
        
        # Sample (Smart)
        print("Sample data (first 3 rows, truncated):")
        cursor.execute(f"SELECT * FROM {table_name} LIMIT 3;")
        rows = cursor.fetchall()
        
        if cursor.description:
            columns = [description[0] for description in cursor.description]
            print(f"Columns: {columns}")
        
        for row in rows:
            truncated_row = []
            for item in row:
                s_item = str(item)
                if len(s_item) > 100:
                    truncated_row.append(s_item[:100] + "... [TRUNCATED]")
                else:
                    truncated_row.append(item)
            print(truncated_row)

    conn.close()
except Exception as e:
    print(f"An error occurred: {e}")
