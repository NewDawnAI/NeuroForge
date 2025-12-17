#!/usr/bin/env python3
import argparse
import os
import sys
import csv
from collections import Counter

# Optional deps
try:
    import pandas as pd  # type: ignore
except Exception:
    pd = None

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt  # type: ignore
except Exception:
    matplotlib = None
    plt = None


def find_logs_dir(preferred: str | None) -> str | None:
    if preferred:
        p = os.path.abspath(preferred)
        if os.path.isdir(p):
            return p
        return None
    # Common candidates relative to repo root
    candidates = [
        os.path.join('build', 'Release', 'PhaseC_Logs'),
        os.path.join('build', 'PhaseC_Logs'),
        'PhaseC_Logs',
    ]
    for c in candidates:
        if os.path.isdir(c):
            return os.path.abspath(c)
    return None


def load_csv_as_dicts(path: str):
    rows = []
    with open(path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append(r)
    return rows


def load_csv(path: str):
    if pd is not None:
        try:
            return pd.read_csv(path)
        except Exception:
            pass
    # Fallback to list of dicts
    return load_csv_as_dicts(path)


def to_series_int(values):
    # Helper to cast a sequence of values to ints safely
    out = []
    for v in values:
        try:
            out.append(int(v))
        except Exception:
            try:
                out.append(int(float(v)))
            except Exception:
                out.append(0)
    return out


def ensure_dir(path: str):
    os.makedirs(path, exist_ok=True)


def plot_accuracy(seq, out_dir: str):
    if plt is None:
        return None
    # We expect columns: step,target,predicted,correct
    if pd is not None and hasattr(seq, 'to_dict'):
        steps = seq['step'].tolist() if 'step' in seq.columns else list(range(len(seq)))
        correct = to_series_int(seq['correct'].tolist()) if 'correct' in seq.columns else [0]*len(steps)
    else:
        steps = [int(r.get('step', i)) for i, r in enumerate(seq)]
        correct = to_series_int([r.get('correct', 0) for r in seq])

    # Rolling accuracy (window=20)
    window = 20
    rolling = []
    acc_sum = 0
    for i, c in enumerate(correct):
        acc_sum += c
        if i >= window:
            acc_sum -= correct[i-window]
            rolling.append(acc_sum / window)
        else:
            rolling.append(acc_sum / (i+1))

    fig, ax = plt.subplots(figsize=(8, 4))
    ax.plot(steps, rolling, label=f'Rolling accuracy (w={window})', color='#1f77b4')
    ax.scatter(steps, correct, s=6, alpha=0.4, label='Per-step correct', color='#ff7f0e')
    ax.set_xlabel('Step')
    ax.set_ylabel('Accuracy / Correct')
    ax.set_title('Accuracy over time')
    ax.legend(loc='lower right')
    fig.tight_layout()
    out_path = os.path.join(out_dir, 'accuracy_over_time.svg')
    fig.savefig(out_path, format='svg')
    plt.close(fig)
    return out_path


def plot_winner_timeline(timeline, out_dir: str):
    if plt is None:
        return None, None
    # Columns: step,winner_id,winner_symbol,winner_score
    if pd is not None and hasattr(timeline, 'to_dict'):
        steps = timeline['step'].tolist() if 'step' in timeline.columns else list(range(len(timeline)))
        symbols = timeline['winner_symbol'].fillna('NA').astype(str).tolist() if 'winner_symbol' in timeline.columns else ['NA']*len(steps)
    else:
        steps = [int(r.get('step', i)) for i, r in enumerate(timeline)]
        symbols = [str(r.get('winner_symbol', 'NA')) for r in timeline]

    # Map symbols to category indices
    uniq = sorted(set(symbols))
    idx_map = {s: i for i, s in enumerate(uniq)}
    y = [idx_map[s] for s in symbols]

    # Timeline scatter
    fig1, ax1 = plt.subplots(figsize=(9, 4))
    ax1.scatter(steps, y, s=6, alpha=0.6, c=y, cmap='tab20')
    ax1.set_yticks(range(len(uniq)))
    ax1.set_yticklabels(uniq)
    ax1.set_xlabel('Step')
    ax1.set_ylabel('Winner symbol')
    ax1.set_title('Winner timeline')
    fig1.tight_layout()
    tl_path = os.path.join(out_dir, 'winner_timeline.svg')
    fig1.savefig(tl_path, format='svg')
    plt.close(fig1)

    # Frequency bar chart
    counts = Counter(symbols)
    labels = list(counts.keys())
    vals = [counts[k] for k in labels]

    fig2, ax2 = plt.subplots(figsize=(8, 4))
    ax2.bar(labels, vals, color='#2ca02c')
    ax2.set_ylabel('Count')
    ax2.set_title('Winner symbol frequency')
    ax2.tick_params(axis='x', rotation=45, labelsize=8)
    fig2.tight_layout()
    freq_path = os.path.join(out_dir, 'winner_frequency.svg')
    fig2.savefig(freq_path, format='svg')
    plt.close(fig2)

    return tl_path, freq_path


def plot_assembly_richness(assemblies, out_dir: str):
    if plt is None:
        return None
    # Columns: step,assembly_id,symbol,score
    if pd is not None and hasattr(assemblies, 'to_dict'):
        steps = assemblies['step'].tolist() if 'step' in assemblies.columns else list(range(len(assemblies)))
        asm_ids = assemblies['assembly_id'].tolist() if 'assembly_id' in assemblies.columns else list(range(len(assemblies)))
    else:
        steps = [int(r.get('step', i)) for i, r in enumerate(assemblies)]
        asm_ids = [int(r.get('assembly_id', i)) for i, r in enumerate(assemblies)]

    # Compute cumulative distinct assemblies by step
    zipped = sorted(zip(steps, asm_ids), key=lambda x: x[0])
    seen = set()
    cum = []
    out_steps = []
    for s, a in zipped:
        seen.add(a)
        out_steps.append(s)
        cum.append(len(seen))

    fig, ax = plt.subplots(figsize=(8, 4))
    ax.plot(out_steps, cum, color='#d62728')
    ax.set_xlabel('Step')
    ax.set_ylabel('# Distinct assemblies (cumulative)')
    ax.set_title('Assembly richness over time')
    fig.tight_layout()
    out_path = os.path.join(out_dir, 'assembly_richness.svg')
    fig.savefig(out_path, format='svg')
    plt.close(fig)
    return out_path


def plot_synapse_weights_hist(weights_csv_paths: list[str], out_dir: str):
    if plt is None:
        return None
    weights = []
    for p in weights_csv_paths:
        if not p or not os.path.isfile(p):
            continue
        rows = load_csv_as_dicts(p)
        for r in rows:
            try:
                w = float(r.get('weight', ''))
            except Exception:
                continue
            weights.append(w)
    if not weights:
        return None

    fig, ax = plt.subplots(figsize=(6, 4))
    ax.hist(weights, bins=30, color='#9467bd', alpha=0.85)
    ax.set_xlabel('Weight')
    ax.set_ylabel('Count')
    ax.set_title('Synapse weight distribution')
    fig.tight_layout()
    out_path = os.path.join(out_dir, 'synapse_weights_hist.svg')
    fig.savefig(out_path, format='svg')
    plt.close(fig)
    return out_path


def write_summary(logs_dir: str, seq, timeline, assemblies, weight_paths: list[str]):
    # Compute basic stats
    if pd is not None and hasattr(seq, 'to_dict'):
        total_seq = len(seq)
        sum_correct = int(seq['correct'].fillna(0).astype(int).sum()) if 'correct' in seq.columns else 0
    else:
        total_seq = len(seq)
        sum_correct = sum(int(r.get('correct', 0)) for r in seq)

    acc = (sum_correct / total_seq) if total_seq else 0.0

    if pd is not None and hasattr(timeline, 'to_dict'):
        steps = len(timeline)
        symbols = timeline['winner_symbol'].fillna('NA').astype(str).tolist() if 'winner_symbol' in timeline.columns else []
    else:
        steps = len(timeline)
        symbols = [str(r.get('winner_symbol', 'NA')) for r in timeline]

    top_winners = Counter(symbols).most_common(5)

    if pd is not None and hasattr(assemblies, 'to_dict'):
        distinct_assemblies = assemblies['assembly_id'].nunique() if 'assembly_id' in assemblies.columns else 0
    else:
        distinct_assemblies = len(set(int(r.get('assembly_id', -1)) for r in assemblies))

    # Weights count
    weights_found = 0
    for p in weight_paths:
        if p and os.path.isfile(p):
            try:
                with open(p, 'r', encoding='utf-8') as f:
                    weights_found += max(0, sum(1 for _ in f) - 1)  # minus header
            except Exception:
                pass

    out_path = os.path.join(logs_dir, 'summary.txt')
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('Phase C Analysis Summary\n')
        f.write('========================\n')
        f.write(f'Logs directory: {logs_dir}\n')
        f.write(f'Total sequence rows: {total_seq}\n')
        f.write(f'Overall accuracy: {acc*100:.2f}%\n')
        f.write(f'Timeline steps: {steps}\n')
        f.write(f'Distinct assemblies: {distinct_assemblies}\n')
        if top_winners:
            f.write('Top winners (symbol: count):\n')
            for sym, cnt in top_winners:
                f.write(f'  - {sym}: {cnt}\n')
        f.write(f'Total weights observed in live synapse snapshots: {weights_found}\n')
    return out_path


def main():
    parser = argparse.ArgumentParser(description='Analyze Phase C run outputs and generate plots + summary.')
    parser.add_argument('--logs-dir', type=str, default=None, help='Path to PhaseC_Logs directory. If omitted, attempts auto-detect.')
    parser.add_argument('--live-synapses', type=str, default=None, help='Path to live_synapses.csv (optional). If omitted, tries to find common locations.')
    args = parser.parse_args()

    logs_dir = find_logs_dir(args.logs_dir)
    if not logs_dir:
        print('ERROR: Could not locate PhaseC_Logs directory. Use --logs-dir to specify.', file=sys.stderr)
        sys.exit(1)

    ensure_dir(logs_dir)

    # Resolve CSV paths
    seq_path = os.path.join(logs_dir, 'sequence.csv')
    tl_path = os.path.join(logs_dir, 'timeline.csv')
    asm_path = os.path.join(logs_dir, 'assemblies.csv')

    # Load data
    seq = load_csv(seq_path) if os.path.isfile(seq_path) else []
    timeline = load_csv(tl_path) if os.path.isfile(tl_path) else []
    assemblies = load_csv(asm_path) if os.path.isfile(asm_path) else []

    # Generate plots
    if seq:
        plot_accuracy(seq, logs_dir)
    if timeline:
        plot_winner_timeline(timeline, logs_dir)
    if assemblies:
        plot_assembly_richness(assemblies, logs_dir)

    # Synapses histogram
    weight_candidates = []
    if args.live_synapses:
        weight_candidates.append(args.live_synapses)
    # Common locations for live_synapses.csv
    for p in [
        os.path.join(os.getcwd(), 'live_synapses.csv'),
        os.path.abspath(os.path.join(os.getcwd(), '..', 'live_synapses.csv')),
        os.path.abspath(os.path.join(os.path.dirname(logs_dir), '..', 'live_synapses.csv')),
    ]:
        if p not in weight_candidates:
            weight_candidates.append(p)
    plot_synapse_weights_hist(weight_candidates, logs_dir)

    # Summary
    write_summary(logs_dir, seq, timeline, assemblies, weight_candidates)

    print(f'Analysis complete. Outputs saved in: {logs_dir}')


if __name__ == '__main__':
    main()