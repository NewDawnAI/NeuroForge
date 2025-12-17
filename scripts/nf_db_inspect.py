import sqlite3, json, os

db_path = r"c:\Users\ashis\Desktop\NeuroForge\2m_neuron_simulation.db"
conn = sqlite3.connect(db_path)
conn.row_factory = sqlite3.Row
cur = conn.cursor()

print(f"DB: {db_path}")
# List tables
tables = [r[0] for r in cur.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name").fetchall()]
print(f"Tables ({len(tables)}): {', '.join(tables)}")

# Counts per table
for t in tables:
    try:
        cnt = cur.execute(f"SELECT COUNT(*) FROM {t}").fetchone()[0]
        print(f"Count[{t}]={cnt}")
    except Exception as e:
        print(f"Count[{t}]=<error {e}>")

# Schemas (short)
for t in tables:
    try:
        schema = cur.execute("SELECT sql FROM sqlite_master WHERE type='table' AND name=?", (t,)).fetchone()[0]
        # Truncate overly long schema for readability
        s = schema if len(schema) <= 300 else schema[:280] + "..."
        print(f"Schema[{t}]: {s}")
    except Exception as e:
        print(f"Schema[{t}]=<error {e}>")

# Helper to sample rows
def sample(table, limit=5):
    try:
        cols = [r[1] for r in cur.execute(f"PRAGMA table_info({table})").fetchall()]
        order_by = None
        if 'ts_ms' in cols and 'step' in cols:
            order_by = 'ts_ms DESC, step DESC'
        elif 'ts_ms' in cols:
            order_by = 'ts_ms DESC'
        elif 'id' in cols:
            order_by = 'id DESC'
        sql = f"SELECT * FROM {table}"
        if order_by:
            sql += f" ORDER BY {order_by}"
        sql += f" LIMIT {limit}"
        rows = cur.execute(sql).fetchall()
        print(f"Sample[{table}] ({len(rows)} rows):")
        for r in rows:
            print(json.dumps(dict(r), ensure_ascii=False))
    except Exception as e:
        print(f"Sample[{table}]=<error {e}>")

key_tables = ['runs','learning_stats','episodes','episode_stats','experiences','substrate_states','hippocampal_snapshots','reward_log','self_model']
for t in key_tables:
    if t in tables:
        sample(t)

# Summaries
if 'learning_stats' in tables:
    row = cur.execute(
        """
        SELECT 
          MAX(step) AS max_step,
          AVG(processing_hz) AS avg_hz,
          SUM(total_updates) AS total_updates,
          SUM(hebbian_updates) AS hebbian_updates,
          SUM(stdp_updates) AS stdp_updates,
          SUM(reward_updates) AS reward_updates,
          AVG(avg_weight_change) AS avg_weight_change,
          AVG(consolidation_rate) AS avg_consolidation,
          MAX(active_synapses) AS max_active_synapses,
          MAX(potentiated_synapses) AS max_potentiated_synapses,
          MAX(depressed_synapses) AS max_depressed_synapses
        FROM learning_stats
        """
    ).fetchone()
    print("Summary[learning_stats]:", json.dumps(dict(row)))

if 'substrate_states' in tables:
    by_type = cur.execute("SELECT state_type, COUNT(*) AS cnt FROM substrate_states GROUP BY state_type ORDER BY cnt DESC").fetchall()
    by_region = cur.execute("SELECT region_id, COUNT(*) AS cnt FROM substrate_states GROUP BY region_id ORDER BY cnt DESC").fetchall()
    top10 = cur.execute("SELECT state_type, region_id, COUNT(*) AS cnt FROM substrate_states GROUP BY state_type, region_id ORDER BY cnt DESC LIMIT 10").fetchall()
    print("substrate_states by type:", json.dumps([(r[0], r[1]) for r in by_type]))
    print("substrate_states by region:", json.dumps([(r[0], r[1]) for r in by_region]))
    print("Top (type, region):", json.dumps([(r[0], r[1], r[2]) for r in top10]))

if 'hippocampal_snapshots' in tables:
    row = cur.execute("SELECT COUNT(*) AS cnt, MAX(step) AS max_step, AVG(significance) AS avg_sig, AVG(priority) AS avg_prio FROM hippocampal_snapshots").fetchone()
    print("Summary[hippocampal_snapshots]:", json.dumps(dict(row)))

if 'episodes' in tables:
    cnt_ep = cur.execute("SELECT COUNT(*) FROM episodes").fetchone()[0]
    print(f"Episodes count: {cnt_ep}")
if 'episode_stats' in tables:
    sums = cur.execute("SELECT SUM(steps) AS steps, SUM(success) AS successes, SUM(episode_return) AS returns FROM episode_stats").fetchone()
    print("Episode stats sums:", json.dumps(dict(sums)))

if 'reward_log' in tables:
    row = cur.execute("SELECT COUNT(*) AS cnt, AVG(reward) AS avg_reward, MIN(reward) AS min_reward, MAX(reward) AS max_reward FROM reward_log").fetchone()
    print("Summary[reward_log]:", json.dumps(dict(row)))

if 'self_model' in tables:
    row = cur.execute("SELECT COUNT(*) AS cnt FROM self_model").fetchone()
    print("Self_model entries:", row[0])

conn.close()
