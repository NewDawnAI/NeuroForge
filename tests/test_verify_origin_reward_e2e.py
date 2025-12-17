import os, sys, sqlite3, time
import unittest

ROOT = os.path.abspath(os.getcwd())
OUT_DIR = os.path.join(ROOT, 'build', 'out')
if OUT_DIR not in sys.path:
    sys.path.insert(0, OUT_DIR)

import phase_c_workspace as m

class TestVerifyOriginRewardE2E(unittest.TestCase):
    def setUp(self):
        self.db_path = os.path.join(ROOT, 'test_verify_origin.sqlite')
        try:
            os.remove(self.db_path)
        except Exception:
            pass
        self.bus = m.AgentBus(validator_mode='strict')
        self.curator = m.MemoryCurator(db_path=self.db_path, name='Curator')
        self.curator.subscribe_all(self.bus)
        self.planner = m.PlannerAgent(bus=self.bus, period=3, name='Planner')
        self.planner.subscribe(self.bus)

    def tearDown(self):
        try:
            self.curator.close()
        except Exception:
            pass

    def test_verify_rewards_emitted(self):
        # Emit 3 winners to trigger a plan
        winners = [('A', 0.6), ('B', 0.7), ('C', 0.8)]
        for i, (sym, score) in enumerate(winners):
            self.bus.publish('winner', {'step': i, 'agent': 'Tester', 'payload': {'winner_symbol': sym, 'winner_score': score}})
        # Send verify events for three statuses
        statuses = ['confirmed', 'invalidated', 'adjusted']
        for j, st in enumerate(statuses, start=len(winners)):
            self.bus.publish('verify', {'step': j, 'agent': 'Verifier', 'payload': {'status': st}})
        # Allow async commit
        time.sleep(0.02)
        con = sqlite3.connect(self.db_path)
        cur = con.cursor()
        # Find verify-origin rewards: agent Planner, topic reward, payload has plan_id and no symbol
        cur.execute(
            """
            SELECT COUNT(*) FROM messages
            WHERE topic='reward' AND agent='Planner'
              AND json_extract(payload_json, '$.plan_id') IS NOT NULL
              AND json_extract(payload_json, '$.symbol') IS NULL
            """
        )
        cnt = cur.fetchone()[0]
        con.close()
        self.assertGreaterEqual(cnt, 3, 'expected at least 3 verify-origin rewards (one per status)')

if __name__ == '__main__':
    unittest.main()