import importlib.util
import os
import sys
import time


def load_module_from_path(module_name: str, file_path: str):
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load spec for {module_name} from {file_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    root = os.path.dirname(os.path.abspath(__file__))
    ws_path = os.path.join(root, 'build', 'out', 'phase_c_workspace.py')
    mod = load_module_from_path('phase_c_workspace', ws_path)

    # Initialize bus and agents
    bus = mod.AgentBus(validator_mode='strict')

    # Instantiate agents
    # MemoryCurator expects paths, not a bus
    db_path = os.path.join(root, 'smoke_phase_c.sqlite')
    curator = mod.MemoryCurator(db_path=db_path, csv_path=None, name='MemoryCurator')
    language = mod.LanguageAgent(bus)
    planner = mod.PlannerAgent(bus, period=2)
    critic = mod.CriticAgent(bus)
    bridge = mod.EmergentBridge(bus)
    perception = mod.PerceptionAgent(bus)

    # Subscriptions
    curator.subscribe_all(bus)
    language.subscribe(bus)
    planner.subscribe(bus)
    critic.subscribe(bus)
    bridge.subscribe(bus)

    # Drive a small sequence of events
    step = 1
    perception.publish_percept(step, {'text': 'hello world'})

    # Publish a simple binding_map to exercise CriticAgent metrics
    bus.publish('binding_map', {
        'step': step,
        'agent': 'smoke',
        'payload': {
            'map': {
                'color': {'red': 0.72, 'blue': 0.62},
                'shape': {'circle': 0.81, 'square': 0.3}
            }
        }
    })

    # Winners to trigger PlannerAgent plan emission (period=2)
    bus.publish('winner', {
        'step': step,
        'agent': 'smoke',
        'payload': {'winner_symbol': 'A', 'winner_score': 0.8}
    })
    step += 1
    bus.publish('winner', {
        'step': step,
        'agent': 'smoke',
        'payload': {'winner_symbol': 'B', 'winner_score': 0.7}
    })

    # Verify the last plan to also produce a reward via verify_map
    bus.publish('verify', {
        'step': step,
        'agent': 'smoke',
        'payload': {'status': 'confirmed', 'reason': 'ok'}
    })

    # Give async handlers (if any) a moment
    time.sleep(0.1)
    print('SMOKE: completed without exceptions')


if __name__ == '__main__':
    main()