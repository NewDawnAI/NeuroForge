import sqlite3, sys, os
p = r"C:\Users\ashis\Desktop\NeuroForge\phasec_mem.db"
print("DB path:", p, "exists:", os.path.exists(p))
con = sqlite3.connect(p)
cur = con.cursor()
tables = [r[0] for r in cur.execute("SELECT name FROM sqlite_master WHERE type='table'")]
print("tables:", tables)

def count(table):
    try:
        return cur.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
    except Exception as e:
        print(f"Error counting {table}:", e)
        return None

for t in ["runs","episodes","learning_stats","reward_log","experiences","episode_stats"]:
    print(t, count(t))