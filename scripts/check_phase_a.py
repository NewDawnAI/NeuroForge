import sqlite3, json, sys

db_path = 'phase5_baby_multimodal.sqlite'
con = sqlite3.connect(db_path)
cur = con.cursor()
cur.execute('SELECT id, step, context_json FROM reward_log ORDER BY id DESC LIMIT 3;')
rows = cur.fetchall()

print(f"Checking latest reward_log rows in {db_path}")
print("=" * 50)

for r in rows:
    print(f"id={r[0]} step={r[1]}")
    try:
        j = json.loads(r[2])
        keys = sorted(list(j.keys()))
        print(f"  JSON keys: {keys[:10]}{'...' if len(keys) > 10 else ''}")
        print(f"  has phase_a nested: {'phase_a' in j}")
        if 'phase_a' in j:
            phase_a_keys = list(j['phase_a'].keys()) if isinstance(j['phase_a'], dict) else []
            print(f"  phase_a nested keys: {phase_a_keys}")
        print(f"  has phase_a_last_reward: {'phase_a_last_reward' in j}")
        print(f"  has language nested: {'language' in j}")
        print(f"  has self nested: {'self' in j}")
    except Exception as e:
        print(f"  JSON error: {e}")
        print(f"  Raw (first 200 chars): {r[2][:200]}")
    print("-" * 30)

con.close()