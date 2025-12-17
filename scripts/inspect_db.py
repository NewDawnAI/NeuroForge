import os, sqlite3, json, sys, traceback

DB_NAME = os.environ.get("NF_DB", "phase5_baby_multimodal.sqlite")

print("DB exists:", os.path.exists(DB_NAME), "path=", os.path.abspath(DB_NAME))
if not os.path.exists(DB_NAME):
    sys.exit(1)

con = sqlite3.connect(DB_NAME)
con.row_factory = sqlite3.Row
cur = con.cursor()

# List tables
try:
    tables = [r[0] for r in cur.execute("SELECT name FROM sqlite_master WHERE type='table';").fetchall()]
    print("Tables:", tables)
except Exception:
    print("Failed to list tables:")
    traceback.print_exc()

# Helper to show schema
def show_schema(name: str):
    try:
        cols = cur.execute(f"PRAGMA table_info({name});").fetchall()
        print(f"Schema for {name}:")
        for c in cols:
            # cid, name, type, notnull, dflt_value, pk
            print("  ", dict(c))
    except Exception:
        print(f"Failed to get schema for {name}")
        traceback.print_exc()

# Helper to count rows
def table_count(name: str) -> int:
    try:
        return cur.execute(f"SELECT COUNT(1) FROM {name};").fetchone()[0]
    except Exception:
        return -1

print("\n-- table counts --")
for t in [
    'runs','learning_stats','experiences','episodes','episode_experiences','episode_stats','reward_log','self_model'
]:
    if t in tables:
        print(f" {t}:", table_count(t))

# Show schemas
for t in ['reward_log','experiences','self_model']:
    if t in tables:
        show_schema(t)

# Probe reward_log JSON/context-like columns
print("\n-- reward_log (latest 5) --")
if 'reward_log' in tables:
    try:
        # Determine potential JSON column
        cols = [r[1] for r in cur.execute("PRAGMA table_info(reward_log);").fetchall()]
        json_col = None
        for cand in ['ctx','context','context_json','json','data','meta']:
            if cand in cols:
                json_col = cand
                break
        base_cols = ', '.join([c for c in ['id','ts_ms','step','reward'] if c in cols])
        select_cols = base_cols + (f", {json_col}" if json_col else '')
        rows = cur.execute(f"SELECT {select_cols} FROM reward_log ORDER BY id DESC LIMIT 5;").fetchall()
        print("count:", len(rows), "json_col:", json_col)
        for r in rows:
            rd = dict(r)
            print(" id=", rd.get('id'), "step=", rd.get('step'), "reward=", rd.get('reward'))
            if json_col:
                raw = rd.get(json_col)
                try:
                    j = json.loads(raw) if isinstance(raw, (str,bytes)) else raw
                    keys = list(j.keys()) if isinstance(j, dict) else []
                    print("   json keys:", keys[:15])
                    # Common probes
                    def has_path(d, path):
                        curd = d
                        for k in path:
                            if not isinstance(curd, dict) or k not in curd:
                                return False
                            curd = curd[k]
                        return True
                    for k in ["phase_a","language","self","phase_a_last_reward","language_stage","self_awareness"]:
                        print("   has", k, ":", (k in j) if isinstance(j, dict) else False)
                    print("   has phase_a.last_reward:", has_path(j,["phase_a","last_reward"]) if isinstance(j, dict) else False)
                    print("   has language.metrics:", has_path(j,["language","metrics"]) if isinstance(j, dict) else False)
                    print("   has self.state:", has_path(j,["self","state"]) if isinstance(j, dict) else False)
                except Exception as e:
                    print("   json parse error:", e)
    except Exception:
        traceback.print_exc()

print("\n-- experiences (latest 5) --")
if 'experiences' in tables:
    try:
        rows = cur.execute("SELECT id, step, tag, input_json FROM experiences ORDER BY id DESC LIMIT 5;").fetchall()
        print("count:", len(rows))
        for r in rows:
            js = r[3]
            try:
                j = json.loads(js) if isinstance(js, (str, bytes)) else js
                tname = type(j).__name__
            except Exception as e:
                print(f" id={r[0]} step={r[1]} tag={r[2]} json_error={e}")
                continue
            top_keys = list(j.keys())[:15] if isinstance(j, dict) else []
            def has_path(d, path):
                curd = d
                for k in path:
                    if not isinstance(curd, dict) or k not in curd:
                        return False
                    curd = curd[k]
                return True
            print(f" id={r[0]} step={r[1]} tag={r[2]} type={tname} keys={top_keys}")
            if isinstance(j, dict):
                for k in ["phase_a", "language", "self"]:
                    print("   has", k, ":", (k in j))
                print("   has phase_a.last_reward:", has_path(j, ["phase_a", "last_reward"]))
                print("   has language.metrics:", has_path(j, ["language", "metrics"]))
                print("   has self.state:", has_path(j, ["self", "state"]))
    except Exception:
        traceback.print_exc()

print("\n-- self_model (latest 5) --")
if 'self_model' in tables:
    try:
        rows = cur.execute("SELECT * FROM self_model ORDER BY id DESC LIMIT 5;").fetchall()
        print("count:", len(rows))
        for r in rows:
            print(dict(r))
    except Exception:
        traceback.print_exc()

con.close()
print("\nDone.")