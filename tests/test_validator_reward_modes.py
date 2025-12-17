import os, sys, sqlite3, time

# Ensure module import
ROOT = os.path.abspath(os.getcwd())
OUT_DIR = os.path.join(ROOT, 'build', 'out')
if OUT_DIR not in sys.path:
    sys.path.insert(0, OUT_DIR)

import phase_c_workspace as m

def run_case(mode: str, db_path: str):
    if os.path.exists(db_path):
        try:
            os.remove(db_path)
        except Exception:
            pass
    bus = m.AgentBus(validator_mode=mode)
    curator = m.MemoryCurator(db_path=db_path, name=f'Curator-{mode}')
    curator.subscribe_all(bus)

    # Publish malformed reward: wrong types and missing fields
    bus.publish('reward', {
        'step': 1,
        'agent': 'Tester',
        'schema_id': 'phase_c.v1.reward',
        'payload': {
            # novelty missing, confidence wrong type, missing reward_scalar, uncertainty missing
            'confidence': 'high',
        }
    })

    # Give time and close
    time.sleep(0.05)
    curator.close()

    con = sqlite3.connect(db_path)
    cur = con.cursor()
    cur.execute("SELECT COUNT(*) FROM messages WHERE topic='system';")
    system_count = cur.fetchone()[0]
    cur.execute("SELECT COUNT(*) FROM messages WHERE topic='reward';")
    reward_count = cur.fetchone()[0]
    con.close()
    return system_count, reward_count


def main():
    strict_db = os.path.join(ROOT, 'test_validator_strict.sqlite')
    off_db = os.path.join(ROOT, 'test_validator_off.sqlite')
    s_sys, s_rw = run_case('strict', strict_db)
    o_sys, o_rw = run_case('off', off_db)
    print('STRICT -> system:', s_sys, 'reward:', s_rw)
    print('OFF    -> system:', o_sys, 'reward:', o_rw)
    # Expectations: strict should block invalid reward (system>=1, reward==0); off should warn and still deliver (system>=1, reward>=1)
    assert s_sys >= 1 and s_rw == 0, 'strict mode should block invalid reward'
    assert o_sys >= 1 and o_rw >= 1, 'off mode should warn and still deliver invalid reward'

if __name__ == '__main__':
    main()