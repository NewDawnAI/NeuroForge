import sqlite3, os
paths = ['./src/test_queries.sqlite','./test_queries.sqlite','./memory.sqlite','./test_integration.sqlite']
for p in paths:
    if not os.path.exists(p):
        print(f'{p}: MISSING')
        continue
    print(f'=== {p} ===')
    con = sqlite3.connect(p)
    cur = con.cursor()
    try:
        cur.execute('select id, started_ms, metadata_json from runs order by id')
        print('runs:', cur.fetchall())
    except Exception as e:
        print('runs query failed:', e)
    try:
        cur.execute('select id, run_id, name, start_ms, coalesce(end_ms,0) from episodes order by id')
        rows = cur.fetchall()
        print('episodes:', len(rows))
        if rows:
            print('episodes sample:', rows[:5])
    except Exception as e:
        print('episodes query failed:', e)
    try:
        cur.execute('select id, run_id, ts_ms, step, reward, source from reward_log order by id')
        rows = cur.fetchall()
        print('rewards:', len(rows))
        if rows:
            print('rewards sample:', rows[:5])
    except Exception as e:
        print('rewards query failed:', e)
    con.close()
