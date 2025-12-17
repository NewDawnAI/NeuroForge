#!/usr/bin/env python3
import argparse
import csv
import os
import sys
from collections import defaultdict, Counter
from statistics import mean


def load_csv_rows(path):
    rows = []
    with open(path, newline='', encoding='utf-8') as f:
        r = csv.DictReader(f)
        for row in r:
            rows.append(row)
    return rows


def coerce_int(x, default=None):
    try:
        return int(x)
    except Exception:
        return default


def coerce_float(x, default=None):
    try:
        return float(x)
    except Exception:
        return default


def analyze_wm(wm_rows):
    # Normalize rows
    norm = []
    roles = set()
    for row in wm_rows:
        step = coerce_int(row.get('step'))
        role = (row.get('role') or '').strip()
        filler = (row.get('filler') or '').strip()
        strength = coerce_float(row.get('strength'))
        if step is None or strength is None or not role:
            continue
        roles.add(role)
        norm.append((step, role, filler, strength))

    # Group by step
    by_step = defaultdict(list)
    for step, role, filler, strength in norm:
        by_step[step].append((role, filler, strength))

    steps_sorted = sorted(by_step.keys())

    # Counts per role per step and avg strength per role per step
    counts = {}            # step -> Counter(role -> count)
    avg_strength = {}      # step -> dict(role -> avg strength)
    token_counts = []      # list of (step, token_count)

    for s in steps_sorted:
        items = by_step[s]
        c = Counter()
        accum = defaultdict(list)
        for role, filler, strength in items:
            c[role] += 1
            accum[role].append(strength)
        counts[s] = c
        avg_strength[s] = {r: (mean(vals) if vals else 0.0) for r, vals in accum.items()}
        token_counts.append((s, c.get('token', 0)))

    # Aggregate summaries
    token_count_values = [tc for _, tc in token_counts]
    max_tokens = max(token_count_values) if token_count_values else 0
    avg_tokens = mean(token_count_values) if token_count_values else 0.0

    # Per-role average strength time series
    roles_sorted = sorted(roles)
    role_series = {r: [] for r in roles_sorted}
    for s in steps_sorted:
        for r in roles_sorted:
            role_series[r].append(avg_strength.get(s, {}).get(r, 0.0))

    return {
        'steps': steps_sorted,
        'counts': counts,
        'avg_strength': avg_strength,
        'roles': roles_sorted,
        'role_series': role_series,
        'token_counts': token_counts,
        'max_tokens': max_tokens,
        'avg_tokens': avg_tokens,
    }


def analyze_sequence(seq_rows):
    # Expect columns: step,target,predicted,correct
    correct_vals = []
    for row in seq_rows:
        c = coerce_int(row.get('correct'))
        if c is None:
            # try parse as bool-like string
            val = (row.get('correct') or '').strip().lower()
            if val in ('1', 'true', 'yes'): c = 1
            elif val in ('0', 'false', 'no'): c = 0
        if c is not None:
            correct_vals.append(int(c))
    acc = mean(correct_vals) if correct_vals else None
    return {'accuracy': acc, 'n': len(correct_vals)}


def maybe_plot(log_dir, steps, role_series, token_counts, seq_window_hint=None):
    try:
        import matplotlib
        matplotlib.use('Agg')  # headless
        import matplotlib.pyplot as plt
    except Exception:
        print('[analyze] matplotlib not available — skipping plot generation', file=sys.stderr)
        return None

    fig, axes = plt.subplots(2, 1, figsize=(8, 6), constrained_layout=True)

    # Plot 1: Avg strength per role over time
    ax = axes[0]
    for role, series in role_series.items():
        ax.plot(steps, series, label=role)
    ax.set_title('Average WM Strength by Role')
    ax.set_xlabel('Step')
    ax.set_ylabel('Avg Strength')
    ax.set_ylim(0, 1.05)
    ax.grid(True, alpha=0.3)
    ax.legend(loc='best')

    # Plot 2: Token counts per step + seq-window hint
    ax2 = axes[1]
    if token_counts:
        xs = [s for s, _ in token_counts]
        ys = [c for _, c in token_counts]
        ax2.plot(xs, ys, color='tab:blue', label='token count per step')
        if seq_window_hint and seq_window_hint > 0:
            ax2.axhline(seq_window_hint, color='tab:red', linestyle='--', label=f'seq-window={seq_window_hint}')
    ax2.set_title('Token Entries per Step (role=token)')
    ax2.set_xlabel('Step')
    ax2.set_ylabel('# token entries')
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='best')

    out_path = os.path.join(log_dir, 'PhaseC_Analysis.svg')
    fig.savefig(out_path, format='svg')
    print(f'[analyze] Wrote plot: {out_path}')
    return out_path


def main():
    parser = argparse.ArgumentParser(description='Analyze Phase C Working Memory logs (CSV)')
    parser.add_argument('--log-dir', type=str, default=os.path.join('build', 'Release', 'PhaseC_Logs'),
                        help='Directory containing working_memory.csv, sequence.csv')
    parser.add_argument('--plot', action='store_true', help='Generate SVG plots into log-dir')
    parser.add_argument('--seq-window', type=int, default=None, help='Optional: annotate plot with expected seq-window cap')
    args = parser.parse_args()

    wm_csv = os.path.join(args.log_dir, 'working_memory.csv')
    seq_csv = os.path.join(args.log_dir, 'sequence.csv')

    if not os.path.isfile(wm_csv):
        print(f'[analyze] ERROR: working_memory.csv not found at: {wm_csv}', file=sys.stderr)
        return 2

    wm_rows = load_csv_rows(wm_csv)
    wm = analyze_wm(wm_rows)

    print('=== Working Memory Summary ===')
    print(f'Log dir: {args.log_dir}')
    print(f'Steps observed: {len(wm["steps"])}, Roles: {", ".join(wm["roles"]) or "(none)"}')
    print(f'Token entries per step: max={wm["max_tokens"]}, avg={wm["avg_tokens"]:.2f}')

    # Print first few steps breakdown
    preview_n = min(5, len(wm['steps']))
    print('\nPer-step counts (first {0}):'.format(preview_n))
    for s in wm['steps'][:preview_n]:
        c = wm['counts'][s]
        counts_str = ', '.join(f'{r}:{c[r]}' for r in wm['roles'] if c.get(r, 0) > 0)
        print(f'  step {s}: {counts_str}')

    # Average strengths (first few steps)
    print('\nAvg strengths by role (first {0} steps):'.format(preview_n))
    for s in wm['steps'][:preview_n]:
        avg = wm['avg_strength'][s]
        av_str = ', '.join(f'{r}:{avg.get(r, 0.0):.3f}' for r in wm['roles'])
        print(f'  step {s}: {av_str}')

    # Sequence accuracy if available
    if os.path.isfile(seq_csv):
        seq_rows = load_csv_rows(seq_csv)
        seq = analyze_sequence(seq_rows)
        if seq['accuracy'] is not None:
            print(f"\nSequence accuracy: {seq['accuracy']*100:.1f}% over {seq['n']} steps")
        else:
            print('\nSequence accuracy: (not available)')
    else:
        print('\n(sequence.csv not found — skipping sequence accuracy)')

    if args.plot:
        maybe_plot(args.log_dir, wm['steps'], wm['role_series'], wm['token_counts'], args.seq_window)

    return 0


if __name__ == '__main__':
    sys.exit(main())