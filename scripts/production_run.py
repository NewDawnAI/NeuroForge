#!/usr/bin/env python3
"""
Run NeuroForge with recommended production flags for a unified substrate session.

Examples:
  python scripts/production_run.py --exe build/neuroforge.exe --db build/phasec_mem.db --steps 6000 --interval 500 --learn --hebb 0.01 --stdp 0.005
"""
import argparse
import subprocess
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description='NeuroForge production runner')
    ap.add_argument('--exe', default=str(Path('build') / 'neuroforge.exe'))
    ap.add_argument('--db', default=str(Path('build') / 'phasec_mem.db'))
    ap.add_argument('--steps', type=int, default=5000)
    ap.add_argument('--interval', type=int, default=500, help='memdb interval ms')
    ap.add_argument('--learn', action='store_true', help='enable learning')
    ap.add_argument('--hebb', type=float, default=0.01)
    ap.add_argument('--stdp', type=float, default=0.005)
    ap.add_argument('--wm', type=int, default=256)
    ap.add_argument('--phasec', type=int, default=256)
    ap.add_argument('--adaptive', default='on', choices=['on','off'])
    ap.add_argument('--survival', default='on', choices=['on','off'])
    args = ap.parse_args()

    exe = Path(args.exe).resolve()
    if not exe.exists():
        raise SystemExit(f"ERROR: exe not found: {exe}")
    db = Path(args.db).resolve()
    cmd = [str(exe),
           '--unified-substrate=on',
           f'--wm-neurons={args.wm}', f'--phasec-neurons={args.phasec}',
           f'--memory-db={db}', f'--memdb-interval={args.interval}', f'--steps={args.steps}',
           f'--adaptive={args.adaptive}', f'--survival-bias={args.survival}']
    if args.learn:
        cmd += ['--enable-learning', f'--hebbian-rate={args.hebb}', f'--stdp-rate={args.stdp}']
    print('[production] Running:', ' '.join(cmd))
    r = subprocess.run(cmd)
    raise SystemExit(r.returncode)


if __name__ == '__main__':
    main()

