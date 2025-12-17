#!/usr/bin/env python3
import argparse
import os
import sys
import subprocess
from pathlib import Path
from statistics import mean
from typing import List, Dict, Any

# Reuse analyzer utilities to compute metrics/plots
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # add repo root to path
try:
    from scripts.analyze_phase_c_wm import load_csv_rows, analyze_wm, analyze_sequence, maybe_plot
except Exception as e:
    print(f"[sweep] ERROR: failed to import analyzer utilities: {e}", file=sys.stderr)
    sys.exit(2)


def run_phase_c(exe_path: Path, out_dir: Path, mode: str, steps: int, seed: int,
                wm_capacity: int, wm_decay: float, seq_window: int) -> int:
    out_dir_str = str(out_dir)
    cmd = [
        str(exe_path),
        f"--phase-c=on",
        f"--phase-c-mode={mode}",
        f"--steps={steps}",
        f"--phase-c-out={out_dir_str}",
        f"--phase-c-wm-capacity={wm_capacity}",
        f"--phase-c-wm-decay={wm_decay}",
        f"--phase-c-seq-window={seq_window}",
        f"--phase-c-seed={seed}",
        f"--log-json=off",
    ]
    print("[sweep] Running:", ' '.join(cmd))
    r = subprocess.run(cmd, cwd=str(exe_path.parent), capture_output=True, text=True)
    if r.returncode != 0:
        print("[sweep] ERROR: process failed\nSTDOUT:\n" + r.stdout + "\nSTDERR:\n" + r.stderr, file=sys.stderr)
    else:
        print("[sweep] OK:", r.stdout.strip())
    return r.returncode


def compute_metrics(log_dir: Path, seq_window_hint: int) -> Dict[str, Any]:
    wm_csv = log_dir / 'working_memory.csv'
    seq_csv = log_dir / 'sequence.csv'
    if not wm_csv.exists():
        raise RuntimeError(f"working_memory.csv not found in {log_dir}")

    wm_rows = load_csv_rows(str(wm_csv))
    wm = analyze_wm(wm_rows)
    roles = wm['roles']

    # Overall mean strength per role across steps (avg of per-step averages)
    mean_strength_by_role = {}
    for r in roles:
        series = wm['role_series'][r]
        mean_strength_by_role[r] = (mean(series) if series else 0.0)

    seq_acc = None
    n_seq = 0
    if seq_csv.exists():
        seq_rows = load_csv_rows(str(seq_csv))
        seq = analyze_sequence(seq_rows)
        seq_acc = seq['accuracy']
        n_seq = seq['n']

    return {
        'max_token_entries': wm['max_tokens'],
        'avg_token_entries': wm['avg_tokens'],
        'mean_strength_by_role': mean_strength_by_role,
        'sequence_accuracy': seq_acc,
        'sequence_n': n_seq,
        'steps_observed': len(wm['steps']),
        'roles': roles,
        'token_counts': wm['token_counts'],
    }


def write_summary_csv(out_csv: Path, rows: List[Dict[str, Any]]):
    # Flatten mean_strength_by_role into columns like strength_role_token, etc.
    # Determine all role names across rows
    role_names = set()
    for r in rows:
        msbr = r.get('metrics', {}).get('mean_strength_by_role', {})
        role_names.update(msbr.keys())
    role_cols = sorted(role_names)

    headers = [
        'decay', 'seq_window', 'capacity', 'steps', 'seed',
        'max_token_entries', 'avg_token_entries', 'sequence_accuracy', 'sequence_n',
    ] + [f'strength_role_{rc}' for rc in role_cols]

    out_csv.parent.mkdir(parents=True, exist_ok=True)
    with open(out_csv, 'w', encoding='utf-8', newline='') as f:
        f.write(','.join(headers) + '\n')
        for r in rows:
            vals = [
                str(r['decay']), str(r['seq_window']), str(r['capacity']), str(r['steps']), str(r['seed']),
                str(r['metrics'].get('max_token_entries', '')),
                f"{r['metrics'].get('avg_token_entries', ''):.3f}" if r['metrics'].get('avg_token_entries') is not None else '',
                f"{r['metrics'].get('sequence_accuracy'):.3f}" if r['metrics'].get('sequence_accuracy') is not None else '',
                str(r['metrics'].get('sequence_n', '')),
            ]
            msbr = r['metrics'].get('mean_strength_by_role', {})
            for rc in role_cols:
                v = msbr.get(rc)
                vals.append(f"{v:.3f}" if v is not None else '')
            f.write(','.join(vals) + '\n')
    print(f"[sweep] Wrote summary: {out_csv}")


def main():
    parser = argparse.ArgumentParser(description='Phase C parameter sweep harness')
    parser.add_argument('--exe', type=str, default=str(Path('build') / 'Release' / 'neuroforge.exe'), help='Path to neuroforge.exe')
    parser.add_argument('--base-out', type=str, default=str(Path('build') / 'Release' / 'Sweeps'), help='Base output directory for sweep runs')
    parser.add_argument('--mode', type=str, default='sequence', choices=['binding','sequence'], help='Phase C mode to run')
    parser.add_argument('--steps', type=int, default=20, help='Steps per run')
    parser.add_argument('--seed', type=int, default=42, help='RNG seed')
    parser.add_argument('--capacity', type=int, default=6, help='WorkingMemory capacity (fixed for this sweep)')
    parser.add_argument('--decays', type=float, nargs='+', default=[0.7, 0.85, 0.95, 1.0], help='List of WM decay values to sweep')
    parser.add_argument('--seq-windows', type=int, nargs='+', default=[0, 3, 5], help='List of seq-window values to sweep')
    parser.add_argument('--plots', action='store_true', help='Generate per-run SVG plots')
    parser.add_argument('--capacities', type=int, nargs='+', default=None, help='List of WM capacities to sweep (overrides --capacity)')
    args = parser.parse_args()

    exe_path = Path(args.exe).resolve()
    if not exe_path.exists():
        print(f"[sweep] ERROR: exe not found at {exe_path}", file=sys.stderr)
        return 2

    base_out = Path(args.base_out).resolve()
    base_out.mkdir(parents=True, exist_ok=True)

    summary_rows = []

    capacities = args.capacities if args.capacities else [args.capacity]

    for cap in capacities:
        for d in args.decays:
            for w in args.seq_windows:
                run_dir = base_out / f"mode_{args.mode}_cap_{cap}_decay_{d:.2f}_win_{w}"
                # Run Phase C
                rc = run_phase_c(exe_path, run_dir, args.mode, args.steps, args.seed, int(cap), float(d), int(w))
                if rc != 0:
                    print(f"[sweep] Skipping metrics due to run failure for cap={cap}, decay={d}, win={w}", file=sys.stderr)
                    continue

                # Compute metrics
                metrics = compute_metrics(run_dir, w)

                # Per-run plot
                if args.plots:
                    try:
                        # Load for plotting: reusing analyzer's internal structure
                        wm_rows = load_csv_rows(str(run_dir / 'working_memory.csv'))
                        wm = analyze_wm(wm_rows)
                        maybe_plot(str(run_dir), wm['steps'], wm['role_series'], wm['token_counts'], w)
                    except Exception as e:
                        print(f"[sweep] WARN: plotting failed for {run_dir}: {e}", file=sys.stderr)

                # Append to summary
                summary_rows.append({
                    'decay': d,
                    'seq_window': w,
                    'capacity': cap,
                    'steps': args.steps,
                    'seed': args.seed,
                    'metrics': metrics,
                })

    # Write aggregate summary CSV under base_out
    if summary_rows:
        out_csv = base_out / 'sweep_summary.csv'
        write_summary_csv(out_csv, summary_rows)
        print('[sweep] Done.')
    else:
        print('[sweep] No successful runs to summarize.', file=sys.stderr)
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())