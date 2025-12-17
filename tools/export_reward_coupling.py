#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import json
import csv
import re

HERE = os.path.abspath(os.path.dirname(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
SCRIPTS = os.path.join(ROOT, 'scripts')
ANALYSIS = os.path.join(ROOT, 'analysis')
PAGES = os.path.join(ROOT, 'pages')
DEFAULT_LEARNING = os.path.join(ROOT, 'Artifacts', 'CSV', 'learning_stats_2m_run.csv')
DEFAULT_REWARD = os.path.join(ROOT, 'Artifacts', 'CSV', 'reward_log_2m_run.csv')

FIGS = [
    'learning_delta_w_vs_time.png',
    'ltp_ltd_ratio.png',
    'reward_plasticity_coupling.png',
    'fig5_reward_coupling.png',
    'reward_coupling_scatter.png',
]


def ensure_dir(path: str):
    os.makedirs(path, exist_ok=True)


def run_analysis(learning_csv: str, reward_csv: str | None, out_dir: str, window: int = 25, max_lag: int = 100, reward_source: str | None = None):
    ensure_dir(out_dir)
    cmd = [
        'python', os.path.join(SCRIPTS, 'NeuroForge_Learning_Analysis.py'),
        '--learning-csv', learning_csv,
        '--out-dir', out_dir,
        '--window', str(window),
        '--max-lag', str(max_lag),
    ]
    if reward_csv:
        cmd += ['--reward-csv', reward_csv]
    if reward_source:
        cmd += ['--reward-source', reward_source]
    print('Running:', ' '.join(cmd))
    subprocess.run(cmd, check=True)
    return os.path.join(out_dir, 'summary.txt')


def parse_summary(summary_path: str):
    data = {}
    if not os.path.exists(summary_path):
        return data
    with open(summary_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line.startswith('Lag peak (steps):'):
                try:
                    data['peak_lag'] = int(line.split(':', 1)[1].strip())
                except Exception:
                    pass
            elif line.startswith('Peak r²:'):
                try:
                    data['peak_r2'] = float(line.split(':', 1)[1].strip())
                except Exception:
                    pass
            elif line.startswith('reward_source:'):
                try:
                    data['reward_source'] = line.split(':', 1)[1].strip()
                except Exception:
                    pass
    return data


def copy_outputs(src_dir: str, dest_dir: str):
    ensure_dir(dest_dir)
    copied = []
    for name in FIGS + ['summary.txt']:
        src = os.path.join(src_dir, name)
        if os.path.exists(src):
            dst = os.path.join(dest_dir, name)
            shutil.copy2(src, dst)
            copied.append(dst)
            print('Copied:', dst)
        else:
            print('Missing (skip):', src)
    return copied


def slugify(text: str) -> str:
    text = text.strip().lower()
    text = re.sub(r'[^a-z0-9\-_]+', '-', text)
    text = re.sub(r'-+', '-', text).strip('-')
    return text or 'unknown'


def list_reward_sources(reward_csv: str) -> list:
    sources = set()
    try:
        with open(reward_csv, 'r', encoding='utf-8') as f:
            rdr = csv.reader(f)
            header = next(rdr, None)
            if not header:
                return []
            # Find 'source' column index (case-insensitive)
            idx = None
            for i, h in enumerate(header):
                if h.strip().lower() == 'source':
                    idx = i
                    break
            if idx is None:
                return []
            # Collect up to 10000 rows for unique sources
            for j, row in enumerate(rdr):
                if idx < len(row):
                    s = (row[idx] or '').strip()
                    if s:
                        sources.add(s)
                if j > 100000:
                    break
    except Exception as e:
        print('list_reward_sources error:', e)
        return []
    return sorted(sources)


def main():
    ap = argparse.ArgumentParser(description='Export reward coupling figures to pages/coupling')
    ap.add_argument('--learning-csv', default=DEFAULT_LEARNING)
    ap.add_argument('--reward-csv', default=DEFAULT_REWARD)
    ap.add_argument('--analysis-out', default=os.path.join(ANALYSIS, 'reward_coupling_demo'))
    ap.add_argument('--pages-out', default=os.path.join(PAGES, 'coupling'))
    ap.add_argument('--window', type=int, default=25)
    ap.add_argument('--max-lag', type=int, default=100)
    ap.add_argument('--per-source', action='store_true', help='Also compute per-source coupling if source column exists')
    args = ap.parse_args()

    # Run overall analysis to produce figures and summary
    summary_path = run_analysis(args.learning_csv, args.reward_csv, args.analysis_out, window=args.window, max_lag=args.max_lag)

    # Copy into pages
    copied = copy_outputs(args.analysis_out, args.pages_out)

    # Emit a small JSON with coupling stats for UI usage
    stats = parse_summary(summary_path)
    json_path = os.path.join(args.pages_out, 'reward_coupling_stats.json')
    with open(json_path, 'w', encoding='utf-8') as jf:
        json.dump(stats, jf, indent=2)
    print('Wrote stats JSON:', json_path)

    per_source_index = []
    if args.per_source:
        sources = list_reward_sources(args.reward_csv)
        print('Reward sources detected:', sources)
        for src in sources:
            slug = slugify(src)
            out_sub = os.path.join(args.analysis_out, f'source_{slug}')
            pages_sub = os.path.join(args.pages_out, 'sources', slug)
            ensure_dir(pages_sub)
            spath = run_analysis(args.learning_csv, args.reward_csv, out_sub, window=args.window, max_lag=args.max_lag, reward_source=src)
            copy_outputs(out_sub, pages_sub)
            sstats = parse_summary(spath)
            # Write per-source stats JSON
            sjson = os.path.join(pages_sub, 'reward_coupling_stats.json')
            with open(sjson, 'w', encoding='utf-8') as jf:
                json.dump(sstats, jf, indent=2)
            print('Wrote per-source stats JSON:', sjson)
            per_source_index.append({
                'name': src,
                'slug': slug,
                'path': f'./sources/{slug}/',
                'figs': {
                    'fig5': f'./sources/{slug}/fig5_reward_coupling.png',
                    'scatter': f'./sources/{slug}/reward_coupling_scatter.png',
                    'legacy': f'./sources/{slug}/reward_plasticity_coupling.png',
                },
                'summary': f'./sources/{slug}/summary.txt',
                'stats': f'./sources/{slug}/reward_coupling_stats.json'
            })
        # Write index JSON
        idx_path = os.path.join(args.pages_out, 'sources', 'per_source_index.json')
        ensure_dir(os.path.dirname(idx_path))
        with open(idx_path, 'w', encoding='utf-8') as jf:
            json.dump({'sources': per_source_index}, jf, indent=2)
        print('Wrote per-source index:', idx_path)

    print('Done. Files:')
    for p in copied:
        print(' -', p)
    print(' -', json_path)
    if per_source_index:
        print('Per-source entries:')
        for e in per_source_index:
            print(' -', e['path'])


if __name__ == '__main__':
    main()