import sqlite3, sys, os, time, json

def seed_db(path):
    if not os.path.exists(path):
        print(f"{path}: MISSING")
        return
    con = sqlite3.connect(path)
    cur = con.cursor()
    # Find or create a run
    cur.execute('select id from runs order by id desc limit 1')
    row = cur.fetchone()
    if row:
        run_id = row[0]
    else:
        cur.execute('insert into runs(started_ms, metadata_json) values(?, ?)', (int(time.time()*1000), json.dumps({"seed":"python"})))
        run_id = cur.lastrowid
    print(f"Using run_id={run_id} for {path}")

    base_ms = int(time.time()*1000)
    # Insert two episodes
    cur.execute('insert into episodes(run_id,name,start_ms) values (?,?,?)', (run_id, 'ep_one', base_ms + 10))
    ep1 = cur.lastrowid
    cur.execute('insert into episodes(run_id,name,start_ms) values (?,?,?)', (run_id, 'ep_two', base_ms + 20))
    ep2 = cur.lastrowid
    # End first episode
    cur.execute('update episodes set end_ms=? where id=?', (base_ms + 110, ep1))

    # Insert 6 rewards
    for i in range(6):
        ts = base_ms + 100 + i*5
        step = 1 + i
        reward = 0.1 * (i + 1)
        source = 'seed'
        ctx = json.dumps({"k": i})
        cur.execute('insert into reward_log(run_id, ts_ms, step, reward, source, context_json) values (?,?,?,?,?,?)',
                    (run_id, ts, step, reward, source, ctx))

    con.commit()
    # Show counts
    cur.execute('select count(*) from episodes where run_id=?', (run_id,))
    print('episodes_count=', cur.fetchone()[0])
    cur.execute('select count(*) from reward_log where run_id=?', (run_id,))
    print('rewards_count=', cur.fetchone()[0])
    con.close()

if __name__ == '__main__':
    paths = sys.argv[1:]
    if not paths:
        print('Usage: python nf_seed_db.py <db1> [<db2> ...]')
        sys.exit(1)
    for p in paths:
        try:
            seed_db(p)
        except Exception as e:
            print(f'Error seeding {p}:', e)
