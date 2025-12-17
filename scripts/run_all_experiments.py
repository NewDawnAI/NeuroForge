#!/usr/bin/env python3
"""
One-click orchestration for NeuroForge experiments and cross-model benchmarking.

Steps:
 1) Run unified substrate harness to generate benchmark series JSON/PNGs
 2) Extract Transformer embeddings (text and/or CLIP vision)
 3) Invoke tools/analyze.py with RSA/CKA for each embedding vs all series
 4) Collate CSVs into Artifacts/SUMMARY/all_results.csv

Usage:
  python scripts/run_all_experiments.py \
    --exe build/neuroforge.exe \
    --db build/phasec_mem.db \
    --models bert-base-uncased openai/clip-vit-base-patch32 \
    --inputs data/text.txt data/images.txt \
    --steps 1500 --plots --rsa --cka

Notes:
 - Text models will map to the first text input file; CLIP models map to the first image list file.
 - Ensure Python deps for extract/analyze: transformers, torch, pillow, scipy, matplotlib.
"""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def run_harness(exe: Path, db: Path, steps: int, memdb_interval: int, scale: list, plots: bool) -> None:
    cmd = [
        sys.executable,
        str(Path('scripts') / 'benchmark_unified.py'),
        '--exe', str(exe),
        '--db', str(db),
        '--steps', str(steps),
        '--memdb-interval', str(memdb_interval),
        '--scale', *[str(s) for s in (scale or [256, 512])],
    ]
    if plots:
        cmd.append('--plots')
    print('[orchestrator] Running harness:', ' '.join(cmd))
    r = subprocess.run(cmd, cwd=str(Path(__file__).resolve().parent.parent), capture_output=True, text=True)
    if r.returncode != 0:
        print('[orchestrator] ERROR: harness failed\nSTDOUT:\n' + r.stdout + '\nSTDERR:\n' + r.stderr)
    else:
        print('[orchestrator] Harness OK')


def sanitize(name: str) -> str:
    return ''.join(c if c.isalnum() or c in ('-', '_') else '_' for c in name)


def is_clip_model(name: str) -> bool:
    n = name.lower()
    return 'clip' in n or 'vit' in n


def extract_embeddings(models: list, inputs: list, out_dir: Path) -> list:
    """Map models to input files and run extractor. Returns list of embedding JSON paths."""
    repo_root = Path(__file__).resolve().parent.parent
    out_dir.mkdir(parents=True, exist_ok=True)
    text_inputs = [p for p in inputs if any(p.lower().endswith(ext) for ext in ('.txt', '.jsonl')) and 'image' not in p.lower()]
    image_inputs = [p for p in inputs if 'image' in p.lower() or p.lower().endswith('.txt') and 'image' in Path(p).stem.lower()]
    emb_paths = []

    for m in models:
        m_san = sanitize(m)
        if is_clip_model(m):
            if not image_inputs:
                print(f"[orchestrator] WARN: no image inputs provided for model {m}")
                continue
            inp = image_inputs[0]
            out = out_dir / f'emb_{m_san}.json'
            cmd = [sys.executable, str(Path('tools') / 'extract_transformer.py'), '--model', m, '--inputs', inp, '--out', str(out), '--modality', 'vision']
        else:
            if not text_inputs:
                print(f"[orchestrator] WARN: no text inputs provided for model {m}")
                continue
            inp = text_inputs[0]
            out = out_dir / f'emb_{m_san}_all_layers.json'
            cmd = [sys.executable, str(Path('tools') / 'extract_transformer.py'), '--model', m, '--inputs', inp, '--out', str(out), '--all-layers']
        print('[orchestrator] Extract:', ' '.join(cmd))
        r = subprocess.run(cmd, cwd=str(repo_root), capture_output=True, text=True)
        if r.returncode != 0:
            print('[orchestrator] ERROR: extractor failed\nSTDOUT:\n' + r.stdout + '\nSTDERR:\n' + r.stderr)
        else:
            print('[orchestrator] Extract OK:', r.stdout.strip())
            emb_paths.append(str(out))
    return emb_paths


def find_series_json(root: Path) -> list:
    return [str(p) for p in (root / 'Artifacts' / 'JSON' / 'benchmarks').rglob('*_series.json')]


def analyze_series(series_paths: list, emb_paths: list, out_dir: Path, rsa: bool, cka: bool) -> None:
    repo_root = Path(__file__).resolve().parent.parent
    if not series_paths:
        # Fallback to web substrate_states
        web_series = repo_root / 'web' / 'substrate_states.json'
        if web_series.exists():
            series_paths = [str(web_series)]
    if not series_paths:
        print('[orchestrator] WARN: no series JSON found to analyze')
        return
    for emb in emb_paths or [None]:
        cmd = [sys.executable, str(Path('tools') / 'analyze.py'), '--series', *series_paths, '--out-dir', str(repo_root / 'Artifacts')]
        if rsa:
            cmd.append('--rsa')
        if cka:
            cmd.append('--cka')
        if emb:
            cmd.extend(['--transformer-json', emb])
        print('[orchestrator] Analyze:', ' '.join(cmd))
        r = subprocess.run(cmd, cwd=str(repo_root), capture_output=True, text=True)
        if r.returncode != 0:
            print('[orchestrator] ERROR: analyze failed\nSTDOUT:\n' + r.stdout + '\nSTDERR:\n' + r.stderr)
        else:
            print('[orchestrator] Analyze OK:', r.stdout.strip())


def collate_csvs(summary_out: Path) -> None:
    repo_root = Path(__file__).resolve().parent.parent
    csv_root = repo_root / 'Artifacts' / 'CSV' / 'analysis'
    summary_out.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    # analysis_summary.csv
    as_path = csv_root / 'analysis_summary.csv'
    if as_path.exists():
        import csv
        for i, row in enumerate(csv.reader(open(as_path, 'r', encoding='utf-8'))):
            if i == 0:
                continue
            # file, rows, steps_min, steps_max, coh_mean, coh_var, asm_final, growth_total
            rows.append({'experiment': 'core', 'file': row[0], 'metric': 'coh_mean', 'value': row[4]})
            rows.append({'experiment': 'core', 'file': row[0], 'metric': 'coh_var', 'value': row[5]})
            rows.append({'experiment': 'core', 'file': row[0], 'metric': 'asm_final', 'value': row[6]})
            rows.append({'experiment': 'core', 'file': row[0], 'metric': 'growth_total', 'value': row[7]})
    # rsa_layers_*.csv and cka_layers_*.csv
    for p in csv_root.glob('rsa_layers_*.csv'):
        import csv
        for i, row in enumerate(csv.reader(open(p, 'r', encoding='utf-8'))):
            if i == 0:
                continue
            rows.append({'experiment': 'rsa_layers', 'file': p.stem, 'layer': row[0], 'metric': 'rsa_spearman', 'value': row[1]})
    for p in csv_root.glob('cka_layers_*.csv'):
        import csv
        for i, row in enumerate(csv.reader(open(p, 'r', encoding='utf-8'))):
            if i == 0:
                continue
            rows.append({'experiment': 'cka_layers', 'file': p.stem, 'layer': row[0], 'metric': 'cka', 'value': row[1]})
    # rsa_*.csv and cka_*.csv
    for p in csv_root.glob('rsa_*.csv'):
        import csv
        for i, row in enumerate(csv.reader(open(p, 'r', encoding='utf-8'))):
            if i == 0:
                continue
            rows.append({'experiment': 'rsa', 'file': row[0], 'metric': 'rsa_spearman', 'value': row[2]})
    for p in csv_root.glob('cka_*.csv'):
        import csv
        for i, row in enumerate(csv.reader(open(p, 'r', encoding='utf-8'))):
            if i == 0:
                continue
            rows.append({'experiment': 'cka', 'file': row[0], 'metric': 'cka', 'value': row[2]})

    # Write summary CSV
    import csv
    with open(summary_out, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['experiment', 'file', 'layer', 'metric', 'value'])
        for r in rows:
            w.writerow([r.get('experiment',''), r.get('file',''), r.get('layer',''), r.get('metric',''), r.get('value','')])
    print(f"[orchestrator] Wrote collated summary → {summary_out} ({len(rows)} rows)")


def main():
    ap = argparse.ArgumentParser(description='Run all experiments and analyses end-to-end')
    ap.add_argument('--exe', default=str(Path('build') / 'neuroforge.exe'))
    ap.add_argument('--db', default=str(Path('build') / 'phasec_mem.db'))
    ap.add_argument('--models', nargs='+', default=['bert-base-uncased'])
    ap.add_argument('--inputs', nargs='+', default=[str(Path('data') / 'text.txt')])
    ap.add_argument('--steps', type=int, default=1500)
    ap.add_argument('--memdb-interval', type=int, default=500)
    ap.add_argument('--scale', type=int, nargs='+', default=[256, 512])
    ap.add_argument('--plots', action='store_true')
    ap.add_argument('--rsa', action='store_true')
    ap.add_argument('--cka', action='store_true')
    args = ap.parse_args()

    exe = Path(args.exe).resolve(); db = Path(args.db).resolve()
    out_root = Path('Artifacts').resolve()
    out_root.mkdir(parents=True, exist_ok=True)
    emb_out_dir = out_root / 'JSON' / 'transformers'

    # 1) Harness
    run_harness(exe, db, args.steps, args.memdb_interval, args.scale, args.plots)

    # 2) Extract embeddings
    emb_paths = extract_embeddings(args.models, args.inputs, emb_out_dir)

    # 3) Analyze
    series_paths = find_series_json(Path(__file__).resolve().parent.parent)
    analyze_series(series_paths, emb_paths, out_root, args.rsa, args.cka)

    # 4) Collate CSVs
    collate_csvs(out_root / 'SUMMARY' / 'all_results.csv')

    # 5) Derived time-series metrics
    try:
        cmd_tsm = [sys.executable, str(Path('tools') / 'time_series_metrics.py'), '--series-dir', str(out_root / 'JSON' / 'benchmarks'), '--out', str(out_root / 'CSV' / 'derived'), '--window', '300']
        print('[orchestrator] Derived metrics:', ' '.join(cmd_tsm))
        r = subprocess.run(cmd_tsm, cwd=str(repo_root), capture_output=True, text=True)
        if r.returncode == 0:
            print('[orchestrator] Derived metrics OK:', r.stdout.strip())
        else:
            print('[orchestrator] WARN: derived metrics failed\nSTDOUT:\n' + r.stdout + '\nSTDERR:\n' + r.stderr)
    except Exception as e:
        print('[orchestrator] WARN: derived metrics exception:', e)

    # 6) Statistical tests
    try:
        cmd_stats = [sys.executable, str(Path('tools') / 'stat_tests.py'), '--summary', str(out_root / 'SUMMARY' / 'all_results.csv'), '--out', str(out_root / 'CSV' / 'stats')]
        print('[orchestrator] Statistical tests:', ' '.join(cmd_stats))
        r = subprocess.run(cmd_stats, cwd=str(repo_root), capture_output=True, text=True)
        if r.returncode == 0:
            print('[orchestrator] Statistical tests OK:', r.stdout.strip())
        else:
            print('[orchestrator] WARN: statistical tests failed\nSTDOUT:\n' + r.stdout + '\nSTDERR:\n' + r.stderr)
    except Exception as e:
        print('[orchestrator] WARN: statistical tests exception:', e)

    # 7) Regenerate report
    try:
        cmd_report = [sys.executable, str(Path('scripts') / 'generate_report.py')]
        print('[orchestrator] Regenerate report:', ' '.join(cmd_report))
        r = subprocess.run(cmd_report, cwd=str(repo_root), capture_output=True, text=True)
        if r.returncode == 0:
            print('[orchestrator] Report OK:', r.stdout.strip())
        else:
            print('[orchestrator] WARN: report generation failed\nSTDOUT:\n' + r.stdout + '\nSTDERR:\n' + r.stderr)
    except Exception as e:
        print('[orchestrator] WARN: report exception:', e)

    print('[orchestrator] All done.')


if __name__ == '__main__':
    main()
