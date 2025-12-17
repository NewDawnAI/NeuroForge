import os, sys, sqlite3, time

# Ensure module import
ROOT = os.path.abspath(os.getcwd())
OUT_DIR = os.path.join(ROOT, 'build', 'out')
if OUT_DIR not in sys.path:
    sys.path.insert(0, OUT_DIR)

import phase_c_workspace as m

def main():
    db_path = os.path.join(ROOT, 'test_planner_verify.sqlite')
    if os.path.exists(db_path):
        try:
            os.remove(db_path)
        except Exception:
            pass

    bus = m.AgentBus(validator_mode='strict')
    curator = m.MemoryCurator(db_path=db_path, name='Curator')
    curator.subscribe_all(bus)

    critic = m.CriticAgent(bus=bus, name='Critic')
    critic.subscribe(bus)

    planner = m.PlannerAgent(bus=bus, period=3, name='Planner')
    planner.subscribe(bus)

    # Trigger a plan
    for step, (s, sc) in enumerate([('X', 0.5), ('Y', 0.7), ('Z', 0.8)]):
        bus.publish('winner', {
            'step': step,
            'agent': 'Tester',
            'payload': {
                'winner_symbol': s,
                'winner_score': sc,
            }
        })

    # Send verify events across statuses
    bus.publish('verify', {'step': 3, 'agent': 'Verifier', 'payload': {'status': 'confirmed'}})
    bus.publish('verify', {'step': 4, 'agent': 'Verifier', 'payload': {'status': 'invalidated', 'reason': 'test_fail'}})
    bus.publish('verify', {'step': 5, 'agent': 'Verifier', 'payload': {'status': 'adjusted', 'adjustment': {'horizon': 2}}})

    time.sleep(0.1)
    curator.close()

    con = sqlite3.connect(db_path)
    cur = con.cursor()

    # Ensure plans view exists
    cur.execute("SELECT name FROM sqlite_master WHERE type='view' AND name='plans_v';")
    assert cur.fetchone() is not None, 'plans_v should exist'

    cur.execute("SELECT status, COUNT(*) FROM plans_v GROUP BY status ORDER BY status;")
    counts = {row[0]: row[1] for row in cur.fetchall()}
    print('plan status counts:', counts)
    # Expect at least: plan (initial), confirmed, invalidated, adjusted
    assert counts.get('plan', 0) >= 1, 'initial plan event missing'
    assert counts.get('confirmed', 0) >= 1, 'confirmed status missing'
    assert counts.get('invalidated', 0) >= 1, 'invalidated status missing'
    assert counts.get('adjusted', 0) >= 1, 'adjusted status missing'

    # Inspect latest summary matches pattern
    cur.execute("SELECT status, summary FROM plans_v ORDER BY id DESC LIMIT 3;")
    rows = cur.fetchall()
    print('latest plan statuses:', rows)

    con.close()

if __name__ == '__main__':
    main()