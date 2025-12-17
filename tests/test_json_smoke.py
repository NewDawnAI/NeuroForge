import os
import sys
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
EXE_CANDIDATES = [
    BUILD / 'Release' / 'neuroforge.exe',
    BUILD / 'Debug' / 'neuroforge.exe',
    BUILD / 'neuroforge.exe',
]


def _pick_exe() -> Path:
    for p in EXE_CANDIDATES:
        if p.exists():
            return p
    raise FileNotFoundError('neuroforge.exe not found in build/Release, build/Debug, or build/')


def _run(args: list[str]) -> subprocess.CompletedProcess:
    exe = str(_pick_exe())
    cmd = [exe] + args
    return subprocess.run(cmd, capture_output=True, text=True, cwd=str(BUILD))


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


def test_phase_c_json_smoke_no_warnings():
    # Basic Phase C JSON emission to stdout
    cp = _run(['--phase-c=on', '--phase-c-mode=binding', '--log-json=on', '--steps=2'])
    assert cp.returncode == 0, f'non-zero exit: {cp.returncode}\nstdout={cp.stdout}\nstderr={cp.stderr}'
    assert 'Warning: unrecognized option' not in cp.stdout
    assert 'Warning: unrecognized option' not in cp.stderr
    events = _collect_json_lines(cp.stdout)
    assert len(events) >= 1, f'expected at least one JSON line in stdout, got 0\nstdout={cp.stdout}'
    # basic schema spot-check
    for ev in events:
        assert ev.get('version') == 1
        assert ev.get('phase') in ('C', 'A', 'B')  # timeline/assemblies/binding are in C; safety allows others
        assert 'time' in ev


def test_phase_c_filtered_cli_no_warnings_and_files():
    # Filtered run (C:timeline) should complete without warnings; Phase C writes CSV logs
    cp = _run(['--phase-c=on', '--phase-c-mode=binding', '--log-json=on', '--log-json-events=C:timeline', '--log-json-sample=2', '--steps=5'])
    assert cp.returncode == 0, f'non-zero exit: {cp.returncode}\nstdout={cp.stdout}\nstderr={cp.stderr}'
    assert 'Warning: unrecognized option' not in cp.stdout
    assert 'Warning: unrecognized option' not in cp.stderr
    # Check CSV outputs exist in the executable dir (PhaseC_Logs)
    exe_dir = _pick_exe().parent
    logs_dir = exe_dir / 'PhaseC_Logs'
    assert logs_dir.exists(), f'PhaseC_Logs not found at {logs_dir}'
    timeline_csv = logs_dir / 'timeline.csv'
    assert timeline_csv.exists(), f'timeline.csv not found at {timeline_csv}'
    # Ensure file has at least header or data
    content = timeline_csv.read_text(encoding='utf-8', errors='ignore')
    assert len(content.strip()) > 0, 'timeline.csv is empty'


def test_phase_b_filtered_json_smoke():
    # Filter for Phase B episode_start; ensure at least one JSON event appears on stdout
    # Keep steps low; enable learning to exercise B path
    cp = _run(['--phase-a=on', '--enable-learning', '--log-json=on', '--log-json-events=B:episode_start', '--log-json-sample=1', '--steps=3'])
    assert cp.returncode == 0, f'non-zero exit: {cp.returncode}\nstdout={cp.stdout}\nstderr={cp.stderr}'
    assert 'Warning: unrecognized option' not in cp.stdout
    assert 'Warning: unrecognized option' not in cp.stderr
    events = _collect_json_lines(cp.stdout)
    # Expect at least one B event when filtering B:episode_start
    b_events = [e for e in events if e.get('phase') == 'B' and e.get('event') == 'episode_start']
    assert len(b_events) >= 1, f'expected at least one Phase B episode_start JSON event; got events={events}'


def test_phase_a_decision_json_regression_no_teacher():
    # Expect a Phase A decision JSON when Phase A is enabled with no teacher and mirror-mode off
    cp = _run(['--phase-a=on', '--log-json=on', '--steps=0'])
    assert cp.returncode == 0, f'non-zero exit: {cp.returncode}\nstdout={cp.stdout}\nstderr={cp.stderr}'
    assert 'Warning: unrecognized option' not in cp.stdout
    assert 'Warning: unrecognized option' not in cp.stderr
    events = _collect_json_lines(cp.stdout)
    # Find the A:decision event
    a_decisions = [e for e in events if e.get('phase') == 'A' and e.get('event') == 'decision']
    assert len(a_decisions) >= 1, f'expected at least one Phase A decision JSON event; got events={events}'
    ev = a_decisions[0]
    assert ev.get('version') == 1
    payload = ev.get('payload') or {}
    # With no teacher and mirror off, decided_dim should equal default Phase A config embedding_dimension
    # We cannot know the default here, but we can assert presence and that source mentions default config
    assert 'decided_dim' in payload and isinstance(payload['decided_dim'], int) and payload['decided_dim'] > 0
    assert payload.get('mirror_mode') == 'off'
    assert payload.get('mirror_implied_dim') in (0, None)
    assert isinstance(payload.get('source'), str) and 'default' in payload['source']


def test_phase_a_conflict_and_decision_json_regression():
    # Create a temporary teacher embedding file with a length that mismatches mirror vision grid
    import tempfile
    import random
    tf = tempfile.NamedTemporaryFile(delete=False, mode='w', suffix='.txt')
    try:
        # Choose mirror-mode=vision with grid 8x8 => implied 64; make teacher_len=10 to force conflict
        teacher_values = [str(random.random()) for _ in range(10)]
        tf.write(','.join(teacher_values))
        tf.flush()
        args = [
            '--phase-a=on',
            '--log-json=on',
            '--mirror-mode=vision',
            '--vision-grid=8',
            f'--teacher-embed={tf.name}',
            '--steps=0',
        ]
        cp = _run(args)
        assert cp.returncode == 0, f'non-zero exit: {cp.returncode}\nstdout={cp.stdout}\nstderr={cp.stderr}'
        events = _collect_json_lines(cp.stdout)
        # Expect at least one conflict followed by a decision
        conflicts = [e for e in events if e.get('phase') == 'A' and e.get('event') == 'conflict']
        decisions = [e for e in events if e.get('phase') == 'A' and e.get('event') == 'decision']
        assert len(conflicts) >= 1, f'missing Phase A conflict JSON event; events={events}'
        assert len(decisions) >= 1, f'missing Phase A decision JSON event; events={events}'
        c = conflicts[0]
        d = decisions[0]
        assert c.get('version') == 1 and d.get('version') == 1
        cpayload = c.get('payload') or {}
        dpayload = d.get('payload') or {}
        assert cpayload.get('resolution') == 'teacher_wins'
        assert cpayload.get('teacher_len') == 10
        assert cpayload.get('mirror_mode') == 'vision'
        assert cpayload.get('mirror_implied_dim') == 64
        assert dpayload.get('decided_dim') == 10
        assert dpayload.get('mirror_mode') == 'vision'
        assert dpayload.get('mirror_implied_dim') == 64
    finally:
        try:
            os.unlink(tf.name)
        except Exception:
            pass


def test_cross_phase_json_consistency_smoke():
    # Ensure A+B+C all emit valid JSON events in a minimal end-to-end check.
    # Note: Phase C is implemented as an early-exit path in the binary, so it cannot
    # run alongside A/B in a single invocation. We therefore aggregate across two runs.
    # Run 1: A+B
    args_ab = [
        '--phase-a=on',
        '--enable-learning',
        '--log-json=on',
        '--steps=3',
    ]
    cp1 = _run(args_ab)
    assert cp1.returncode == 0, f'non-zero exit: {cp1.returncode}\nstdout={cp1.stdout}\nstderr={cp1.stderr}'
    assert 'Warning: unrecognized option' not in cp1.stdout
    assert 'Warning: unrecognized option' not in cp1.stderr
    events_ab = _collect_json_lines(cp1.stdout)

    # Run 2: C
    args_c = [
        '--phase-c=on',
        '--phase-c-mode=binding',
        '--log-json=on',
        '--steps=3',
    ]
    cp2 = _run(args_c)
    assert cp2.returncode == 0, f'non-zero exit: {cp2.returncode}\nstdout={cp2.stdout}\nstderr={cp2.stderr}'
    assert 'Warning: unrecognized option' not in cp2.stdout
    assert 'Warning: unrecognized option' not in cp2.stderr
    events_c = _collect_json_lines(cp2.stdout)

    # Combine for global schema checks
    events = events_ab + events_c
    assert len(events) >= 1, f'expected at least one JSON line across runs, got 0\nA+B stdout={cp1.stdout}\nC stdout={cp2.stdout}'

    # Global schema checks: version and ISO-8601 time field on all events
    from datetime import datetime
    for ev in events:
        assert ev.get('version') == 1, f'event missing or wrong version: {ev}'
        t = ev.get('time')
        assert isinstance(t, str), f'event missing time field: {ev}'
        try:
            datetime.fromisoformat(t.replace('Z', '+00:00'))
        except Exception:
            raise AssertionError(f'event time not ISO-8601: {t}')

    # Presence checks per phase/event
    a_decisions = [e for e in events_ab if e.get('phase') == 'A' and e.get('event') == 'decision']
    b_episode_starts = [e for e in events_ab if e.get('phase') == 'B' and e.get('event') == 'episode_start']
    c_any = [e for e in events_c if e.get('phase') == 'C']

    assert len(a_decisions) >= 1, f'expected at least one Phase A decision event; A+B events={events_ab}'
    assert len(b_episode_starts) >= 1, f'expected at least one Phase B episode_start event; A+B events={events_ab}'
    assert len(c_any) >= 1, f'expected at least one Phase C event; C events={events_c}'
