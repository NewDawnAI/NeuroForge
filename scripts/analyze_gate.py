import json, sys
from statistics import mean

if len(sys.argv) < 2:
    print("Usage: analyze_gate.py <jsonl_path>")
    sys.exit(2)

path = sys.argv[1]
count_gate = 0
overrides = []
avg_reward = None
rewards = []

with open(path, 'r', encoding='utf-8') as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
        except Exception:
            continue
        ev = obj.get('event')
        phase = obj.get('phase')
        if ev == 'gate' and phase == '6':
            count_gate += 1
            payload = obj.get('payload') or {}
            overrides.append(1 if payload.get('override_applied') in (True, 'true', 1) else 0)
        elif ev == 'reward' and phase == 'B':
            payload = obj.get('payload') or {}
            r = payload.get('reward')
            if isinstance(r, (int, float)):
                rewards.append(float(r))

avg_override = mean(overrides) if overrides else 0.0
avg_reward = mean(rewards) if rewards else 0.0

print(json.dumps({
    'gate_events': count_gate,
    'override_rate': avg_override,
    'avg_reward': avg_reward
}, ensure_ascii=False))