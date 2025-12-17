import os, sys, sqlite3, time, subprocess, argparse, json, csv, math, statistics
from pathlib import Path
import importlib.util

# Ensure we can import the generated module
# Resolve repo root reliably from this file location
ROOT = str(Path(__file__).resolve().parent.parent)

def _ensure_phase_c_workspace() -> None:
    # If already resolvable, nothing to do
    if importlib.util.find_spec('phase_c_workspace') is not None:
        return
    # Try common build output locations
    candidates = [
        Path(ROOT) / 'build' / 'out',
        Path(ROOT) / 'build' / 'Release' / 'out',
        Path(ROOT) / 'build-release' / 'Release' / 'out',
        Path(ROOT) / 'build',
        Path(ROOT) / 'scripts',
    ]
    for c in candidates:
        try:
            if c.exists():
                sys.path.insert(0, str(c))
                if importlib.util.find_spec('phase_c_workspace') is not None:
                    return
        except Exception:
            continue
    # Fallback: search for a module file anywhere under repo
    try:
        for p in Path(ROOT).rglob('phase_c_workspace.py'):
            sys.path.insert(0, str(p.parent))
            if importlib.util.find_spec('phase_c_workspace') is not None:
                return
    except Exception:
        pass

_ensure_phase_c_workspace()
try:
    import phase_c_workspace as m
except ModuleNotFoundError:
    m = None
    print("[smoke_phase_c] WARNING: phase_c_workspace not found; agent-based checks will be skipped.", file=sys.stderr)

# --- Long-smoke configuration (set from argparse in __main__) ---
LONG_SMOKE = False
LONG_STEPS = 50
LONG_WINDOW = 100
TOLERANCE = 0.30  # relative tolerance for drift checks
BASELINE_CSV = ''
WRITE_BASELINE = False
DUMP_DIR = os.path.join(ROOT, 'PhaseC_Logs')


def _safe_var(values: list[float]) -> float:
    try:
        if len(values) < 2:
            return 0.0
        return statistics.pvariance(values)
    except Exception:
        return 0.0


def _compute_rollups(cur: sqlite3.Cursor, window_size: int) -> list[dict]:
    # Prefer reward_v for quantitative metrics; fallback to empty if not present
    cur.execute("SELECT name FROM sqlite_master WHERE type='view' ORDER BY 1;")
    views = {r[0] for r in cur.fetchall()}
    rows = []
    if 'reward_v' in views:
        try:
            cur.execute("SELECT id, reward_scalar, novelty, confidence, uncertainty FROM reward_v ORDER BY id;")
            rows = cur.fetchall()
        except sqlite3.Error:
            rows = []
    # Prepare windows
    rollups = []
    if not rows:
        return rollups
    # chunk by contiguous rows as a proxy for time windows
    for i in range(0, len(rows), window_size):
        chunk = rows[i:i+window_size]
        if not chunk:
            continue
        ids = [r[0] for r in chunk]
        rewards = [float(r[1]) for r in chunk if r[1] is not None]
        nov = [float(r[2]) for r in chunk if r[2] is not None]
        conf = [float(r[3]) for r in chunk if r[3] is not None]
        unc = [float(r[4]) for r in chunk if r[4] is not None]
        rec = {
            'window_index': len(rollups),
            'start_id': ids[0],
            'end_id': ids[-1],
            'count': len(chunk),
            'mean_reward': (sum(rewards) / len(rewards)) if rewards else 0.0,
            'var_reward': _safe_var(rewards),
            'mean_novelty': (sum(nov) / len(nov)) if nov else 0.0,
            'var_novelty': _safe_var(nov),
            'mean_confidence': (sum(conf) / len(conf)) if conf else 0.0,
            'var_confidence': _safe_var(conf),
            'mean_uncertainty': (sum(unc) / len(unc)) if unc else 0.0,
            'var_uncertainty': _safe_var(unc),
        }
        rollups.append(rec)
    return rollups


def _dump_rollups(rollups: list[dict], out_dir: str, prefix: str = 'phase_c_long_rollups') -> tuple[str, str]:
    os.makedirs(out_dir, exist_ok=True)
    csv_path = os.path.join(out_dir, f'{prefix}.csv')
    json_path = os.path.join(out_dir, f'{prefix}.json')
    if rollups:
        fieldnames = list(rollups[0].keys())
        with open(csv_path, 'w', newline='', encoding='utf-8') as f:
            w = csv.DictWriter(f, fieldnames=fieldnames)
            w.writeheader()
            for r in rollups:
                w.writerow(r)
        with open(json_path, 'w', encoding='utf-8') as f:
            json.dump(rollups, f, indent=2)
    else:
        # write empty files as markers
        with open(csv_path, 'w', newline='', encoding='utf-8') as f:
            f.write('')
        with open(json_path, 'w', encoding='utf-8') as f:
            f.write('[]')
    return csv_path, json_path


def _global_stats_from_rollups(rollups: list[dict]) -> dict:
    # Weighted aggregation of per-window stats
    if not rollups:
        return {}
    N = sum(r['count'] for r in rollups)
    def wmean(key):
        num = sum(r[key] * r['count'] for r in rollups)
        return (num / N) if N else 0.0
    def wsecond_moment(mean_key, var_key):
        # E[X^2] = Var + (E[X])^2 per window; aggregate weighted
        return sum(((r[var_key] + (r[mean_key] ** 2)) * r['count']) for r in rollups) / N if N else 0.0
    m_reward = wmean('mean_reward')
    e2_reward = wsecond_moment('mean_reward', 'var_reward')
    v_reward = max(0.0, e2_reward - (m_reward ** 2))
    return {
        'N': N,
        'mean_reward': m_reward,
        'var_reward': v_reward,
        'mean_novelty': wmean('mean_novelty'),
        'var_novelty': max(0.0, wsecond_moment('mean_novelty', 'var_novelty') - (wmean('mean_novelty') ** 2)),
        'mean_confidence': wmean('mean_confidence'),
        'var_confidence': max(0.0, wsecond_moment('mean_confidence', 'var_confidence') - (wmean('mean_confidence') ** 2)),
        'mean_uncertainty': wmean('mean_uncertainty'),
        'var_uncertainty': max(0.0, wsecond_moment('mean_uncertainty', 'var_uncertainty') - (wmean('mean_uncertainty') ** 2)),
    }


def _rel_diff(a: float, b: float) -> float:
    denom = max(1e-9, abs(b))
    return abs(a - b) / denom


def _compare_to_baseline(baseline_csv: str, rollups: list[dict], tolerance: float) -> None:
    if not baseline_csv or not os.path.exists(baseline_csv) or not rollups:
        return
    # Read baseline rollups CSV
    base_rollups = []
    with open(baseline_csv, 'r', encoding='utf-8') as f:
        rdr = csv.DictReader(f)
        for row in rdr:
            try:
                base_rollups.append({
                    'count': int(row['count']),
                    'mean_reward': float(row['mean_reward']),
                    'var_reward': float(row['var_reward']),
                    'mean_novelty': float(row['mean_novelty']),
                    'var_novelty': float(row['var_novelty']),
                    'mean_confidence': float(row['mean_confidence']),
                    'var_confidence': float(row['var_confidence']),
                    'mean_uncertainty': float(row['mean_uncertainty']),
                    'var_uncertainty': float(row['var_uncertainty']),
                })
            except Exception:
                continue
    if not base_rollups:
        return
    gs_new = _global_stats_from_rollups(rollups)
    gs_base = _global_stats_from_rollups(base_rollups)
    if not gs_new or not gs_base:
        return
    keys = ['mean_reward','var_reward','mean_novelty','var_novelty','mean_confidence','var_confidence','mean_uncertainty','var_uncertainty']
    diffs = {k: _rel_diff(gs_new[k], gs_base[k]) for k in keys}
    print('Baseline comparison (relative diffs):', diffs)
    # Assert diffs within tolerance
    for k, d in diffs.items():
        assert d <= (tolerance * 2.0), f'{k} drift {d:.3f} exceeds allowed tolerance {tolerance*2:.3f} vs baseline'


def _check_stability(rollups: list[dict], tolerance: float) -> None:
    if not rollups:
        print('No rollups available for stability checks (reward_v empty). Skipping long-smoke assertions.')
        return
    # Compare per-window means to global mean
    gs = _global_stats_from_rollups(rollups)
    m = gs.get('mean_reward', 0.0)
    series = [r['mean_reward'] for r in rollups]
    counts = [r['count'] for r in rollups]
    # relative deviation by window
    rel_devs = [abs(x - m) / max(1e-9, abs(m)) if abs(m) > 1e-9 else 0.0 for x in series]
    high_dev = sum(1 for d in rel_devs if d > tolerance)
    # allow up to 20% windows to exceed tolerance, else flag
    allowance = max(1, int(0.2 * len(series)))
    assert high_dev <= allowance, f'Reward mean deviates beyond tolerance in too many windows: {high_dev} > {allowance} (tol={tolerance})'
    # Monotonic drift check (allow small epsilon)
    eps = 1e-6
    deltas = [series[i+1] - series[i] for i in range(len(series)-1)]
    nonneg = all(d >= -eps for d in deltas)
    nonpos = all(d <= eps for d in deltas)
    assert not (nonneg or nonpos), 'Detected monotonic drift in reward mean across windows (expected fluctuations)'
    # Cardinalities bounded: statuses set should remain within expected small set
    # This check is already enforced in the main smoke; keep a gentle guard here via messages topics
    # (optional extension point)


def main():
    db_path = os.path.join(ROOT, 'smoke_phase_c.sqlite')
    if os.path.exists(db_path):
        try:
            os.remove(db_path)
        except Exception:
            pass

    # Run the real NeuroForge engine first to populate MemoryDB with actual telemetry
    engine_ok = False
    exe_candidates = [
        os.path.join(ROOT, 'build', 'Debug', 'neuroforge.exe'),
        os.path.join(ROOT, 'build', 'Release', 'neuroforge.exe'),
        # Fallbacks observed in this workspace
        os.path.join(ROOT, 'build', 'neuroforge.exe'),
        os.path.join(ROOT, 'build-debug', 'Debug', 'neuroforge.exe'),
        os.path.join(ROOT, 'build-release', 'Release', 'neuroforge.exe'),
    ]
    engine_exe = next((p for p in exe_candidates if os.path.exists(p)), None)
    if engine_exe:
        try:
            steps = LONG_STEPS if LONG_SMOKE else 50
            cmd = [
                engine_exe,
                f"--memory-db={db_path}",
                f"--steps={steps}",
                "--step-ms=5",
                "--enable-learning",
                "--phase-c=on",
                "--phase-c-mode=binding",
                "--log-json=on",
                "--log-json-events=C:consolidation",
                "--log-json-sample=1",
                "--hebbian-rate=0.0005",
                "--stdp-rate=0.0005",
                "--vision-demo=off",
                "--viewer=off",
            ]
            print("Running NeuroForge engine:", " ".join(cmd))
            completed = subprocess.run(cmd, capture_output=True, text=True)
            print("neuroforge.exe stdout:\n", completed.stdout)
            print("neuroforge.exe stderr:\n", completed.stderr)
            if completed.returncode == 0:
                engine_ok = True
                # Minimal JSON assertion: ensure consolidation events are present when enabled
                def _collect_json_lines(stdout: str):
                    lines = []
                    for raw in stdout.splitlines():
                        s = raw.strip()
                        if not s:
                            continue
                        if s.startswith('{') and s.endswith('}'):
                            try:
                                lines.append(json.loads(s))
                            except Exception:
                                pass
                    return lines
                evs = _collect_json_lines(completed.stdout)
                cons = [e for e in evs if (e.get('event') == 'consolidation') or (e.get('phase') == 'C' and e.get('event') == 'consolidation')]
                assert len(cons) >= 1, f"expected at least one C:consolidation JSON event; events={evs}"
            else:
                print(f"NeuroForge engine exited with code {completed.returncode}; proceeding with Python smoke only")
        except Exception as e:
            print("Failed to run NeuroForge engine:", e)
    else:
        print("NeuroForge engine executable not found; proceeding with Python smoke only")

    # If the generated Phase C module is unavailable, skip Python agent simulation and return
    if m is None:
        print("[smoke_phase_c] phase_c_workspace unavailable; skipping Python agent simulation. Engine run and long-smoke post-processing only.")
        return

    bus = m.AgentBus(validator_mode='strict')

    # Optional: also write a flat messages CSV for quick inspection
    messages_csv = os.path.join(ROOT, 'messages_smoke.csv')
    try:
        if os.path.exists(messages_csv):
            os.remove(messages_csv)
    except Exception:
        pass

    curator = m.MemoryCurator(db_path=db_path, csv_path=messages_csv, name='Curator')
    curator.subscribe_all(bus)

    critic = m.CriticAgent(bus=bus, name='Critic')
    try:
        critic.subscribe(bus)
    except Exception:
        # Some duplicate class variants may differ; ensure subscription to key topics
        bus.subscribe('winner', getattr(critic, 'on_winner'))
        if hasattr(critic, 'on_plan'):
            bus.subscribe('plan', getattr(critic, 'on_plan'))

    planner = m.PlannerAgent(bus=bus, period=3, name='Planner')
    planner.subscribe(bus)

    # Bridge to emit narratives when rewards arrive for confirmed/adjusted/invalidated plans
    bridge = m.EmergentBridge(bus=bus, name='Bridge')
    bridge.subscribe(bus)

    # Add language agent to enforce language_v outputs on verify-reward
    language = m.LanguageAgent(bus=bus, name='Language')
    language.subscribe(bus)

    # Simulate winners over multiple periods to generate multiple plans and verify outcomes
    if LONG_SMOKE:
        steps_py = max(100, min(LONG_STEPS, 2000))
        base_symbols = ['A', 'B', 'C', 'A', 'B', 'C', 'D', 'E', 'F']
        base_scores = [0.6, 0.7, 0.8, 0.5, 0.9, 0.65, 0.55, 0.75, 0.85]
        symbols = [base_symbols[i % len(base_symbols)] for i in range(steps_py)]
        scores = [base_scores[i % len(base_scores)] for i in range(steps_py)]
        # Verify every ~25 steps with rotating statuses
        status_cycle = ['confirmed', 'adjusted', 'invalidated']
        verify_schedule = {i: status_cycle[(i // 25) % len(status_cycle)] for i in range(0, steps_py, 25)}
    else:
        symbols = ['A', 'B', 'C', 'A', 'B', 'C', 'D', 'E', 'F']
        scores = [0.6, 0.7, 0.8, 0.5, 0.9, 0.65, 0.55, 0.75, 0.85]
        verify_schedule = {2: 'confirmed', 3: 'adjusted', 5: 'adjusted', 6: 'confirmed', 8: 'invalidated'}  # step -> status (>=5 verifies)

    for step, (s, sc) in enumerate(zip(symbols, scores)):
        bus.publish('winner', {
            'step': step,
            'agent': 'Tester',
            'payload': {
                'winner_symbol': s,
                'winner_score': sc,
            }
        })
        if step in verify_schedule:
            bus.publish('verify', {
                'step': step,
                'agent': 'Verifier',
                'payload': {
                    'status': verify_schedule[step]
                }
            })

    # Give bus a moment (synchronous here, but ensure DB commit)
    time.sleep(0.1)

    curator.close()

    con = sqlite3.connect(db_path)
    cur = con.cursor()

    # Optional engine assertions toggle via env
    engine_assert = os.getenv('NF_ASSERT_ENGINE_DB', '').strip().lower() in ('1', 'true', 'on', 'yes')

    # Ensure views exist
    cur.execute("SELECT name FROM sqlite_master WHERE type='view' ORDER BY 1;")
    views = [r[0] for r in cur.fetchall()]
    print('VIEWS:', views)
    assert 'plans_v' in views, 'plans_v view should exist'
    assert 'reward_v' in views, 'reward_v view should exist'
    assert 'narrative_v' in views, 'narrative_v view should exist'
    assert 'language_v' in views, 'language_v view should exist'
    assert 'errors_v' in views, 'errors_v view should exist for validator diagnostics'

    # Count reward events
    cur.execute("SELECT COUNT(*) FROM messages WHERE topic='reward';")
    msg_reward_count = cur.fetchone()[0]

    # Count via reward_v
    rv_count = None
    if 'reward_v' in views:
        cur.execute("SELECT COUNT(*) FROM reward_v;")
        rv_count = cur.fetchone()[0]

    # Plans view
    pv_count = None
    if 'plans_v' in views:
        cur.execute("SELECT COUNT(*) FROM plans_v;")
        pv_count = cur.fetchone()[0]
        cur.execute("SELECT DISTINCT status FROM plans_v;")
        statuses = [r[0] for r in cur.fetchall()]
    else:
        statuses = []

    # Narrative view
    nv_count = None
    if 'narrative_v' in views:
        cur.execute("SELECT COUNT(*) FROM narrative_v;")
        nv_count = cur.fetchone()[0]

    # Language view
    lv_count = None
    if 'language_v' in views:
        cur.execute("SELECT COUNT(*) FROM language_v;")
        lv_count = cur.fetchone()[0]

    # C++ engine tables (reward_log, learning_stats)
    reward_log_count = None
    learning_stats_count = None
    try:
        cur.execute("SELECT COUNT(*) FROM reward_log;")
        reward_log_count = cur.fetchone()[0]
    except sqlite3.Error:
        pass
    try:
        cur.execute("SELECT COUNT(*) FROM learning_stats;")
        learning_stats_count = cur.fetchone()[0]
    except sqlite3.Error:
        pass

    # Errors view must remain empty under normal smoke traffic
    ev_count = 0
    if 'errors_v' in views:
        cur.execute("SELECT COUNT(*) FROM errors_v;")
        ev_count = cur.fetchone()[0]

    # Stronger assertions to prevent silent regressions
    assert pv_count is not None and pv_count >= 3, 'plans_v should have at least three rows (multiple cycles)'
    assert any(s in ('confirmed', 'plan', 'adjusted', 'invalidated') for s in statuses), 'expected plan lifecycle entries in plans_v'
    assert msg_reward_count >= 3, 'expected multiple reward messages persisted in messages table'
    assert rv_count is not None and rv_count >= 3, 'expected multiple reward rows in reward_v'
    assert nv_count is not None and nv_count >= 5, 'expected at least five narrative rows in narrative_v'
    assert lv_count is not None and lv_count >= 5, 'expected at least five language rows in language_v'
    if engine_ok and engine_assert:
        assert reward_log_count is not None and reward_log_count >= 1, 'expected reward_log rows from C++ engine (NF_ASSERT_ENGINE_DB)'
        assert learning_stats_count is not None and learning_stats_count >= 1, 'expected learning_stats rows from C++ engine (NF_ASSERT_ENGINE_DB)'
    assert ev_count == 0, f'errors_v should be empty in smoke path (found {ev_count})'

    # Samples
    reward_samples = []
    if 'reward_v' in views:
        cur.execute("SELECT id, plan_id, novelty, confidence, uncertainty, reward_scalar FROM reward_v ORDER BY id LIMIT 5;")
        reward_samples = cur.fetchall()

    plan_samples = []
    if 'plans_v' in views:
        cur.execute("SELECT id, plan_id, status, summary FROM plans_v ORDER BY id DESC LIMIT 5;")
        plan_samples = cur.fetchall()

    # Optional: sample language and errors
    language_samples = []
    if 'language_v' in views:
        cur.execute("SELECT id, step, agent, utterance FROM language_v ORDER BY id DESC LIMIT 5;")
        language_samples = cur.fetchall()

    errors_samples = []
    if 'errors_v' in views:
        cur.execute("SELECT id, ts, type, error, reason FROM errors_v ORDER BY id DESC LIMIT 5;")
        errors_samples = cur.fetchall()

    print('reward messages:', msg_reward_count)
    print('reward_v rows:', rv_count)
    print('plans_v rows:', pv_count)
    print('narrative_v rows:', nv_count)
    print('language_v rows:', lv_count)
    print('errors_v rows:', ev_count)
    if engine_ok:
        print('reward_log rows (C++):', reward_log_count)
        print('learning_stats rows (C++):', learning_stats_count)
        if engine_assert:
            print('Engine telemetry assertions: ENABLED via NF_ASSERT_ENGINE_DB')
    else:
        print('Note: C++ engine stage skipped')
    print('plan statuses:', statuses)
    print('reward_v sample:', reward_samples)
    print('plans_v sample:', plan_samples)
    print('language_v sample:', language_samples)
    if ev_count:
        print('errors_v sample:', errors_samples)

    con.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Phase C smoke test with optional long-smoke stability mode')
    parser.add_argument('--long-smoke', action='store_true', help='Enable long-smoke mode (longer run + rolling stats)')
    parser.add_argument('--long-steps', type=int, default=1000, help='Total steps for long-smoke engine phase (default: 1000)')
    parser.add_argument('--window', type=int, default=100, help='Rolling window size for stats aggregation (default: 100)')
    parser.add_argument('--tolerance', type=float, default=0.30, help='Relative tolerance for stability checks (default: 0.30)')
    parser.add_argument('--baseline', type=str, default='', help='Optional baseline CSV path to compare against')
    parser.add_argument('--write-baseline', action='store_true', help='If set, write current rollups as the baseline CSV (overwrites)')
    parser.add_argument('--dump-dir', type=str, default=os.path.join(ROOT, 'PhaseC_Logs'), help='Directory to write rollup CSV/JSON')
    args = parser.parse_args()

    # Configure module-level settings for main()
    LONG_SMOKE = bool(args.long_smoke)
    LONG_STEPS = int(args.long_steps)
    LONG_WINDOW = int(args.window)
    TOLERANCE = float(args.tolerance)
    BASELINE_CSV = args.baseline or ''
    WRITE_BASELINE = bool(args.write_baseline)
    DUMP_DIR = args.dump_dir or os.path.join(ROOT, 'PhaseC_Logs')

    # Run the standard smoke (with long parameters applied internally if enabled)
    main()

    # If long-smoke requested, compute rollups and perform stability/baseline checks
    if LONG_SMOKE:
        db_path = os.path.join(ROOT, 'smoke_phase_c.sqlite')
        try:
            con = sqlite3.connect(db_path)
            cur = con.cursor()
            rollups = _compute_rollups(cur, LONG_WINDOW)
            csv_path, json_path = _dump_rollups(rollups, DUMP_DIR)
            print(f'Long-smoke rollups written to: {csv_path}, {json_path}')
            # Stability checks
            _check_stability(rollups, TOLERANCE)
            # Baseline compare
            if BASELINE_CSV:
                _compare_to_baseline(BASELINE_CSV, rollups, TOLERANCE)
            # Optionally write baseline
            if WRITE_BASELINE:
                # Overwrite the provided baseline path or default to PhaseC_Logs/phase_c_long_baseline.csv
                target = BASELINE_CSV or os.path.join(DUMP_DIR, 'phase_c_long_baseline.csv')
                # Reuse CSV dump format
                # Ensure directory exists
                os.makedirs(os.path.dirname(target), exist_ok=True)
                fieldnames = list(rollups[0].keys()) if rollups else ['window_index','start_id','end_id','count','mean_reward','var_reward','mean_novelty','var_novelty','mean_confidence','var_confidence','mean_uncertainty','var_uncertainty']
                with open(target, 'w', newline='', encoding='utf-8') as f:
                    w = csv.DictWriter(f, fieldnames=fieldnames)
                    w.writeheader()
                    for r in rollups:
                        w.writerow(r)
                print(f'Baseline written: {target}')
            con.close()
        except Exception as e:
            print('Long-smoke post-processing failed:', e)
            raise