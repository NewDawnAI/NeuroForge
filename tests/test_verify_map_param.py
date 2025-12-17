import os, sys
import unittest

# Ensure module import path (generated build output)
ROOT = os.path.abspath(os.getcwd())
OUT_DIR = os.path.join(ROOT, 'build', 'out')
if OUT_DIR not in sys.path:
    sys.path.insert(0, OUT_DIR)

import phase_c_workspace as m

REQUIRED_VERIFY_STATUSES = {'confirmed', 'invalidated', 'adjusted'}


class TestVerifyMapConfig(unittest.TestCase):
    def test_required_statuses_present(self):
        verify_map = m.REWARD_CONFIG.get('verify_map', {})
        missing = REQUIRED_VERIFY_STATUSES - set(verify_map.keys())
        self.assertFalse(missing, f'REWARD_CONFIG.verify_map missing statuses: {sorted(missing)}')

    def test_values_are_numbers(self):
        verify_map = m.REWARD_CONFIG.get('verify_map', {})
        for status in REQUIRED_VERIFY_STATUSES:
            with self.subTest(status=status):
                cfg = verify_map.get(status)
                self.assertIsInstance(cfg, dict, f'verify_map[{status}] must be a dict')
                self.assertIn('reward_scalar', cfg)
                self.assertIn('confidence', cfg)
                # allow ints/floats/strings that can be coerced to float
                try:
                    rs = float(cfg['reward_scalar'])
                    cf = float(cfg['confidence'])
                except Exception as e:
                    self.fail(f'verify_map[{status}] values must be numeric-coercible: {e}')
                # Optional sanity checks on ranges (non-binding, adjust as needed)
                self.assertTrue(-1e6 < rs < 1e6, 'reward_scalar out of reasonable range')
                self.assertTrue(-1e6 < cf < 1e6, 'confidence out of reasonable range')


if __name__ == '__main__':
    unittest.main()