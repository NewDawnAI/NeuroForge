#!/usr/bin/env python3
"""
Run statistical tests on benchmark metrics to produce publishable tables.

Inputs (flexible):
 - --summary: Artifacts/SUMMARY/all_results.csv (collated metrics like RSA/CKA)
 - --derived: Artifacts/CSV/derived/time_series_metrics.csv (time-to-first-assembly, damping, etc.)
 - Optional RSA permutation: provide --series and --transformer-json to compute RSA and a permutation p-value.

Outputs:
 - CSV: Artifacts/CSV/stats/stat_tests_summary.csv
 - PNG: Artifacts/PNG/stats/stat_tests_boxplots.png
"""
import argparse
import os
from pathlib import Path
import pandas as pd
import numpy as np
from scipy import stats
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import json


def bootstrap_ci(data, n=10000, ci=95):
    data = np.asarray(data, dtype=float)
    if data.size == 0:
        return np.nan, (np.nan, np.nan)
    means = [np.mean(np.random.choice(data, len(data), replace=True)) for _ in range(n)]
    low, high = np.percentile(means, [(100-ci)/2, 100-(100-ci)/2])
    return float(np.mean(data)), (float(low), float(high))


def load_summary(path: Path):
    if not path.exists():
        return None
    try:
        return pd.read_csv(path)
    except Exception:
        return None


def analyze_ab_from_summary(df: pd.DataFrame):
    """Compute A/B differences for adaptive_on vs adaptive_off on RSA/CKA per layer."""
    results = []
    if df is None or df.empty:
        return results
    # Expect columns: experiment, file, layer, metric, value
    # Filter to adaptive_vs_fixed
    ab_df = df[(df['experiment'] == 'rsa_layers') | (df['experiment'] == 'cka_layers') | (df['experiment'] == 'rsa') | (df['experiment'] == 'cka')]
    if ab_df.empty:
        return results
    # Identify condition by file or metric source
    def cond_from_file(x):
        x = str(x)
        if 'adaptive_on' in x:
            return 'on'
        if 'adaptive_off' in x:
            return 'off'
        return None
    ab_df = ab_df.copy()
    ab_df['cond'] = ab_df['file'].apply(cond_from_file)
    ab_df = ab_df.dropna(subset=['cond'])
    # Group by metric+layer
    keys = ['metric', 'layer'] if 'layer' in ab_df.columns else ['metric']
    grouped = ab_df.groupby(keys)
    for key, g in grouped:
        on_vals = g[g['cond'] == 'on']['value'].astype(float).values
        off_vals = g[g['cond'] == 'off']['value'].astype(float).values
        # Require paired lengths
        n = min(len(on_vals), len(off_vals))
        if n >= 2:
            on = on_vals[:n]
            off = off_vals[:n]
            diff = on - off
            mean_diff = float(np.mean(diff))
            t_p = stats.ttest_rel(on, off).pvalue
            try:
                w_p = stats.wilcoxon(on, off).pvalue
            except Exception:
                w_p = np.nan
            m, (lo, hi) = bootstrap_ci(diff)
            rec = {
                'metric': key[0] if isinstance(key, tuple) else key,
                'layer': key[1] if isinstance(key, tuple) and len(key) > 1 else '',
                'mean_diff': mean_diff,
                't_p': t_p,
                'w_p': w_p,
                'boot_low': lo,
                'boot_high': hi,
                'n_pairs': n,
            }
            results.append(rec)
    return results


def analyze_ab_from_derived(df: pd.DataFrame, window: int):
    """Compute A/B differences on time-series metrics from derived CSV."""
    results = []
    if df is None or df.empty:
        return results
    # Identify conditions by series_name
    def cond_from_series(x):
        x = str(x)
        if 'adaptive_on' in x:
            return 'on'
        if 'adaptive_off' in x:
            return 'off'
        return None
    df = df.copy()
    df['cond'] = df['series_name'].apply(cond_from_series)
    df = df.dropna(subset=['cond'])
    # Metrics of interest
    metrics = [f'median_coherence_last_{window}', f'damping_ratio_{window}', 'time_to_first_assembly']
    for metric in metrics:
        on_vals = df[df['cond'] == 'on'][metric].dropna().astype(float).values
        off_vals = df[df['cond'] == 'off'][metric].dropna().astype(float).values
        n = min(len(on_vals), len(off_vals))
        if n >= 2:
            on = on_vals[:n]
            off = off_vals[:n]
            diff = on - off
            mean_diff = float(np.mean(diff))
            t_p = stats.ttest_rel(on, off).pvalue
            try:
                w_p = stats.wilcoxon(on, off).pvalue
            except Exception:
                w_p = np.nan
            m, (lo, hi) = bootstrap_ci(diff)
            results.append({
                'metric': metric,
                'layer': '',
                'mean_diff': mean_diff,
                't_p': t_p,
                'w_p': w_p,
                'boot_low': lo,
                'boot_high': hi,
                'n_pairs': n,
            })
    return results


def compute_rdm(vectors):
    n = len(vectors)
    if n < 2:
        return None
    X = np.array(vectors, dtype=float)
    R = np.zeros((n, n))
    for i in range(n):
        for j in range(n):
            xi, xj = X[i], X[j]
            if np.std(xi) == 0 or np.std(xj) == 0:
                R[i, j] = 1.0
            else:
                r = np.corrcoef(xi, xj)[0, 1]
                R[i, j] = 1.0 - r
    return R


def spearman_corr(a, b):
    from scipy.stats import spearmanr
    return spearmanr(a.flatten(), b.flatten()).correlation


def rsa_permutation_p(series_path: Path, transformer_json: Path, n_perm=1000):
    try:
        payload = json.load(open(series_path, 'r', encoding='utf-8'))
        series = payload.get('series', [])
        t_payload = json.load(open(transformer_json, 'r', encoding='utf-8'))
        t_vecs = t_payload.get('vectors') or []
        # Substrate 4D vectors
        s_vecs = []
        for s in series:
            s_vecs.append([
                float(s.get('avg_coherence') or 0.0),
                float(s.get('assemblies') or 0.0),
                float(s.get('bindings') or 0.0),
                float(s.get('growth_velocity') or 0.0),
            ])
        m = min(len(s_vecs), len(t_vecs))
        if m < 5:
            return None
        s_rdm = compute_rdm(s_vecs[:m])
        t_rdm = compute_rdm(np.array(t_vecs[:m]))
        rho_obs = spearman_corr(s_rdm, t_rdm)
        # Permutation: shuffle transformer vectors labels
        null = []
        idx = np.arange(m)
        for _ in range(n_perm):
            np.random.shuffle(idx)
            t_perm = np.array(t_vecs[:m])[idx]
            t_rdm_perm = compute_rdm(t_perm)
            null.append(spearman_corr(s_rdm, t_rdm_perm))
        null = np.array(null)
        p = float((np.sum(null >= rho_obs) + 1) / (n_perm + 1))
        return {'rho_obs': float(rho_obs), 'p_perm': p, 'n_perm': n_perm}
    except Exception:
        return None


def main():
    ap = argparse.ArgumentParser(description='Run paired and bootstrap tests on benchmark metrics')
    ap.add_argument('--summary', help='Artifacts/SUMMARY/all_results.csv')
    ap.add_argument('--derived', help='Artifacts/CSV/derived/time_series_metrics.csv')
    ap.add_argument('--out', default=str(Path('Artifacts') / 'CSV' / 'stats'))
    ap.add_argument('--series', help='Optional series JSON for RSA permutation test')
    ap.add_argument('--transformer-json', help='Optional transformer embeddings JSON for RSA permutation test')
    ap.add_argument('--window', type=int, default=300)
    args = ap.parse_args()

    out_csv_dir = Path(args.out).resolve()
    out_png_dir = Path(str(out_csv_dir).replace('CSV', 'PNG')).resolve()
    out_csv_dir.mkdir(parents=True, exist_ok=True)
    out_png_dir.mkdir(parents=True, exist_ok=True)

    # Load inputs
    df_summary = load_summary(Path(args.summary)) if args.summary else None
    df_derived = load_summary(Path(args.derived)) if args.derived else None

    results = []
    # A/B from collated summary (RSA/CKA)
    results.extend(analyze_ab_from_summary(df_summary))
    # A/B from derived time-series metrics
    results.extend(analyze_ab_from_derived(df_derived, args.window))

    # Optional RSA permutation
    if args.series and args.transformer_json:
        perm = rsa_permutation_p(Path(args.series), Path(args.transformer_json))
        if perm:
            results.append({'metric': 'rsa_permutation', 'layer': '', 'mean_diff': perm['rho_obs'], 't_p': np.nan, 'w_p': np.nan, 'boot_low': np.nan, 'boot_high': np.nan, 'n_pairs': perm['n_perm'], 'p_perm': perm['p_perm']})

    # Write CSV summary
    out_csv = out_csv_dir / 'stat_tests_summary.csv'
    pd.DataFrame(results).to_csv(out_csv, index=False)

    # Boxplot / bars for mean differences
    if results:
        labels = [f"{r['metric']}{(':'+str(r['layer'])) if r['layer'] else ''}" for r in results]
        vals = [r['mean_diff'] for r in results]
        lows = [r['boot_low'] for r in results]
        highs = [r['boot_high'] for r in results]
        # Compute asymmetric error ranges
        err_low = [v - l if np.isfinite(l) else 0.0 for v, l in zip(vals, lows)]
        err_high = [h - v if np.isfinite(h) else 0.0 for v, h in zip(vals, highs)]
        plt.figure(figsize=(max(8, len(labels)*0.9), 4))
        x = np.arange(len(labels))
        plt.bar(x, vals, yerr=[err_low, err_high], capsize=3)
        plt.xticks(x, labels, rotation=45, ha='right')
        plt.ylabel('Mean difference (on - off)')
        plt.title('A/B differences with bootstrap CIs')
        plt.tight_layout()
        plt.savefig(out_png_dir / 'stat_tests_boxplots.png', dpi=150)
        plt.close()
    print(f"[stats] Wrote: {out_csv} and {out_png_dir / 'stat_tests_boxplots.png'}")


if __name__ == '__main__':
    main()

