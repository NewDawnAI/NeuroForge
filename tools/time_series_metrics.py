#!/usr/bin/env python3
import argparse, os, json, csv
from pathlib import Path
import statistics

def load_series_file(path: Path):
    try:
        payload = json.load(open(path, 'r', encoding='utf-8'))
        return payload.get('series', [])
    except Exception as e:
        print(f"[tsm] WARN: failed to load {path}: {e}")
        return []

def compute_metrics(series, window: int):
    if not series:
        return None
    steps = [s.get('step') for s in series if s.get('step') is not None]
    coh = [float(s.get('avg_coherence') or 0.0) for s in series]
    asm = [float(s.get('assemblies') or 0.0) for s in series]
    bnd = [float(s.get('bindings') or 0.0) for s in series]
    # time-to-first-assembly
    tffa = None
    for s in series:
        if (s.get('assemblies') or 0) > 0 and s.get('step') is not None:
            tffa = int(s.get('step'))
            break
    # median coherence over last window
    w = min(window, len(coh))
    coh_last = coh[-w:] if w > 0 else []
    median_last = statistics.median(coh_last) if coh_last else None
    # damping ratio Var(late)/Var(early)
    w2 = min(window, len(coh))
    early = coh[:w2]
    late = coh[-w2:]
    var_early = statistics.pvariance(early) if len(early) > 1 else 0.0
    var_late = statistics.pvariance(late) if len(late) > 1 else 0.0
    damping = (var_late / var_early) if var_early > 0 else None
    # growth total
    totals = [a + b for (a,b) in zip(asm, bnd)]
    growth_total = (totals[-1] - totals[0]) if len(totals) >= 2 else 0.0
    return {
        'steps_total': len(steps),
        'steps_min': min(steps) if steps else None,
        'steps_max': max(steps) if steps else None,
        'time_to_first_assembly': tffa,
        'median_coherence_last_%d' % window: median_last,
        'damping_ratio_%d' % window: damping,
        'final_assemblies': asm[-1] if asm else None,
        'growth_total': growth_total,
    }

def main():
    ap = argparse.ArgumentParser(description='Compute time-series derived metrics for benchmark series JSON')
    ap.add_argument('--series', nargs='*', help='Paths to series JSON files')
    ap.add_argument('--series-dir', help='Directory to scan for *_series.json')
    ap.add_argument('--out', default=str(Path('Artifacts') / 'CSV' / 'derived'))
    ap.add_argument('--window', type=int, default=300)
    args = ap.parse_args()

    files = []
    if args.series:
        files.extend(args.series)
    if args.series_dir:
        base = Path(args.series_dir)
        if base.exists():
            files.extend([str(p) for p in base.rglob('*_series.json')])
    if not files:
        # Fallback to dashboard series
        fallback = Path('web') / 'substrate_states.json'
        if fallback.exists():
            files.append(str(fallback))

    out_dir = Path(args.out).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    out_csv = out_dir / 'time_series_metrics.csv'

    rows = []
    for f in sorted(set(files)):
        path = Path(f).resolve()
        series = load_series_file(path)
        metrics = compute_metrics(series, args.window)
        if not metrics:
            continue
        series_name = path.stem
        exp = path.parent.name
        row = {
            'file': str(path),
            'series_name': series_name,
            'experiment': exp,
        }
        row.update(metrics)
        rows.append(row)

    # Write CSV
    if rows:
        headers = list(rows[0].keys())
        with open(out_csv, 'w', newline='', encoding='utf-8') as f:
            w = csv.DictWriter(f, fieldnames=headers)
            w.writeheader()
            for r in rows:
                w.writerow(r)
        print(f"[tsm] Wrote derived metrics → {out_csv} ({len(rows)} rows)")
    else:
        print("[tsm] No series found for metrics")

if __name__ == '__main__':
    main()

