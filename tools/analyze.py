#!/usr/bin/env python3
"""
Command-line analysis runner for NeuroForge substrate telemetry.

Functions:
- Load series JSON (substrate_states export)
- Compute summary statistics, write CSV
- Generate PNG plots (coherence, growth velocity, assemblies)
- Optional RSA vs Transformer embeddings (if provided)

Usage:
  python tools/analyze.py --series web/substrate_states.json --out-dir Artifacts
  python tools/analyze.py --series Artifacts/JSON/benchmarks/adaptive_vs_fixed/adaptive_on_series.json --out-dir Artifacts --rsa --transformer-json path/to/embeddings.json

Transformer embeddings JSON format (example):
  { "vectors": [[...], [...], ...], "labels": ["item1", "item2", ...] }
Substrate series JSON format:
  { "series": [{"step":..., "avg_coherence":..., "assemblies":..., "bindings":..., "growth_velocity":...}, ...] }
"""
import argparse
import csv
import json
import os
import statistics
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

def safe_float(x, default=None):
    try:
        return float(x) if x is not None else default
    except Exception:
        return default

def load_series(path: Path):
    payload = json.load(open(path, 'r', encoding='utf-8'))
    return payload.get('series', [])

def summarize(series):
    steps = [s.get('step') for s in series if s.get('step') is not None]
    coh = [safe_float(s.get('avg_coherence'), None) for s in series]
    asm = [safe_float(s.get('assemblies'), 0.0) for s in series]
    bnd = [safe_float(s.get('bindings'), 0.0) for s in series]
    vel = [safe_float(s.get('growth_velocity'), None) for s in series]
    def stats(arr):
        arr = [a for a in arr if a is not None]
        if not arr:
            return {"count": 0, "min": None, "max": None, "mean": None, "variance": None}
        return {"count": len(arr), "min": min(arr), "max": max(arr), "mean": sum(arr)/len(arr), "variance": statistics.pvariance(arr) if len(arr)>1 else 0.0}
    totals = [ (a or 0.0) + (b or 0.0) for a,b in zip(asm,bnd) ]
    total_growth = totals[-1] - totals[0] if len(totals)>=2 else 0.0
    return {
        "rows": len(series),
        "steps_min": min(steps) if steps else None,
        "steps_max": max(steps) if steps else None,
        "coherence": stats(coh),
        "assemblies": stats(asm),
        "bindings": stats(bnd),
        "growth_velocity": stats(vel),
        "total_growth": total_growth,
        "final_assemblies": asm[-1] if asm else None,
    }

def write_plots(series, out_dir: Path, prefix: str = ""):
    out_dir.mkdir(parents=True, exist_ok=True)
    steps = [s.get('step') for s in series]
    coh = [safe_float(s.get('avg_coherence'), 0.0) for s in series]
    vel = [safe_float(s.get('growth_velocity'), 0.0) for s in series]
    asm = [safe_float(s.get('assemblies'), 0.0) for s in series]
    plt.figure(figsize=(10,4)); plt.plot(steps, coh); plt.title('Coherence vs Step'); plt.xlabel('step'); plt.ylabel('avg_coherence'); plt.tight_layout(); plt.savefig(out_dir / f'{prefix}coherence_vs_step.png', dpi=150); plt.close()
    plt.figure(figsize=(10,4)); plt.plot(steps, vel); plt.title('Growth Velocity vs Step'); plt.xlabel('step'); plt.ylabel('Δ(assemblies+bindings)'); plt.tight_layout(); plt.savefig(out_dir / f'{prefix}growth_velocity_vs_step.png', dpi=150); plt.close()
    plt.figure(figsize=(10,4)); plt.plot(steps, asm); plt.title('Assemblies vs Step'); plt.xlabel('step'); plt.ylabel('assemblies'); plt.tight_layout(); plt.savefig(out_dir / f'{prefix}assemblies_vs_step.png', dpi=150); plt.close()

def compute_rdm(vectors):
    # vectors: list of list floats (samples x dims)
    import numpy as np
    n = len(vectors)
    if n == 0:
        return None
    X = np.array(vectors, dtype=float)
    R = np.zeros((n,n))
    for i in range(n):
        for j in range(n):
            xi, xj = X[i], X[j]
            # 1 - Pearson correlation
            if np.std(xi)==0 or np.std(xj)==0:
                R[i,j] = 1.0
            else:
                r = np.corrcoef(xi, xj)[0,1]
                R[i,j] = 1.0 - r
    return R

def spearman_corr(a, b):
    import numpy as np
    from scipy.stats import spearmanr
    A = a.flatten(); B = b.flatten()
    return spearmanr(A, B).correlation

def substrate_vectors_for_rsa(series):
    # Use 4D vector per sample: [coh, asm, bnd, vel]
    vecs = []
    for s in series:
        vecs.append([
            safe_float(s.get('avg_coherence'), 0.0),
            safe_float(s.get('assemblies'), 0.0),
            safe_float(s.get('bindings'), 0.0),
            safe_float(s.get('growth_velocity'), 0.0),
        ])
    return vecs

def compute_linear_cka(X, Y):
    """Linear CKA per Kornblith et al. (2019). X: (n,d1), Y: (n,d2)."""
    import numpy as np
    X = np.asarray(X, dtype=float)
    Y = np.asarray(Y, dtype=float)
    # Center columns
    X = X - X.mean(axis=0, keepdims=True)
    Y = Y - Y.mean(axis=0, keepdims=True)
    # Compute cross-covariance Frobenius inner product
    XY = X.T @ Y
    numerator = (XY ** 2).sum()
    XX = X.T @ X
    YY = Y.T @ Y
    denom = np.sqrt(((XX ** 2).sum()) * ((YY ** 2).sum()))
    if denom == 0:
        return 0.0
    return float(numerator / denom)

def write_csv_summary(out_csv: Path, entries):
    out_csv.parent.mkdir(parents=True, exist_ok=True)
    headers = ["file", "rows", "steps_min", "steps_max", "coh_mean", "coh_var", "asm_final", "growth_total"]
    with open(out_csv, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(headers)
        for e in entries:
            s = e['summary']
            w.writerow([
                e['file'], s['rows'], s['steps_min'], s['steps_max'],
                (s['coherence']['mean'] if s['coherence']['mean'] is not None else ''),
                (s['coherence']['variance'] if s['coherence']['variance'] is not None else ''),
                (s['final_assemblies'] if s['final_assemblies'] is not None else ''),
                s['total_growth'],
            ])

def main():
    ap = argparse.ArgumentParser(description="NeuroForge analysis CLI")
    ap.add_argument('--series', required=True, nargs='+', help='Series JSON file(s) to analyze')
    ap.add_argument('--out-dir', default=str(Path('Artifacts')))
    ap.add_argument('--rsa', action='store_true', help='Compute RSA vs transformer embeddings if provided')
    ap.add_argument('--transformer-json', help='Transformer embeddings JSON path')
    ap.add_argument('--cka', action='store_true', help='Compute CKA vs transformer embeddings if provided')
    ap.add_argument('--probe', action='store_true', help='Run logistic probe accuracy on substrate and transformer embeddings')
    ap.add_argument('--labels-file', help='Optional labels file (one label per line) to use for probe')
    args = ap.parse_args()

    out_root = Path(args.out_dir).resolve()
    png_root = out_root / 'PNG' / 'analysis'
    csv_root = out_root / 'CSV' / 'analysis'
    png_root.mkdir(parents=True, exist_ok=True)
    csv_root.mkdir(parents=True, exist_ok=True)

    entries = []
    for s_path in args.series:
        p = Path(s_path).resolve()
        if not p.exists():
            print(f"[analyze] WARN: missing {p}")
            continue
        series = load_series(p)
        summary = summarize(series)
        # Per-file plots
        subdir = png_root / p.stem
        write_plots(series, subdir)
        entries.append({"file": str(p), "summary": summary})

    # Write roll-up CSV
    if entries:
        write_csv_summary(csv_root / 'analysis_summary.csv', entries)
        print(f"[analyze] Wrote CSV summary to {csv_root / 'analysis_summary.csv'}")

    # Optional RSA
    if (args.rsa or args.cka or args.probe) and args.transformer_json:
        try:
            t_payload = json.load(open(args.transformer_json, 'r', encoding='utf-8'))
            t_vecs = t_payload.get('vectors', [])
            layers_map = t_payload.get('layers')
            import numpy as np
            import matplotlib.pyplot as plt
            for s_path in args.series:
                p = Path(s_path).resolve()
                series = load_series(p)
                s_vecs = substrate_vectors_for_rsa(series)
                # Align sample counts by min length
                if args.rsa and t_vecs:
                    n = min(len(s_vecs), len(t_vecs))
                    if n < 2:
                        print(f"[analyze] RSA skipped for {p}: insufficient samples")
                    else:
                        s_rdm = compute_rdm(s_vecs[:n])
                        t_rdm = compute_rdm(t_vecs[:n])
                        rho = spearman_corr(s_rdm, t_rdm)
                        hm_dir = png_root / 'rsa' / p.stem
                        hm_dir.mkdir(parents=True, exist_ok=True)
                        plt.figure(figsize=(6,5)); plt.imshow(s_rdm, cmap='viridis'); plt.colorbar(); plt.title(f'Substrate RDM (n={n})'); plt.tight_layout(); plt.savefig(hm_dir / 'substrate_rdm.png', dpi=150); plt.close()
                        plt.figure(figsize=(6,5)); plt.imshow(t_rdm, cmap='viridis'); plt.colorbar(); plt.title(f'Transformer RDM (n={n})'); plt.tight_layout(); plt.savefig(hm_dir / 'transformer_rdm.png', dpi=150); plt.close()
                        with open(csv_root / f'rsa_{p.stem}.csv', 'w', newline='', encoding='utf-8') as f:
                            w = csv.writer(f); w.writerow(['file','samples','spearman_rho']); w.writerow([str(p), n, rho])
                        print(f"[analyze] RSA for {p}: rho={rho:.3f}")
                # RSA across layers
                if args.rsa and layers_map:
                    layer_keys = sorted(layers_map.keys(), key=lambda x: int(x))
                    scores = []
                    for lk in layer_keys:
                        lv = layers_map[lk]
                        m = min(len(s_vecs), len(lv))
                        if m < 2:
                            scores.append(float('nan'))
                            continue
                        s_rdm = compute_rdm(s_vecs[:m])
                        t_rdm = compute_rdm(lv[:m])
                        rho = spearman_corr(s_rdm, t_rdm)
                        scores.append(rho)
                    # Plot heatmap of layer RSA (1 x L)
                    hm_dir = png_root / 'rsa_layers' / p.stem
                    hm_dir.mkdir(parents=True, exist_ok=True)
                    plt.figure(figsize=(max(6, len(layer_keys)), 2)); plt.imshow([scores], aspect='auto', cmap='magma'); plt.colorbar(); plt.title('RSA vs layers'); plt.yticks([]); plt.xticks(range(len(layer_keys)), layer_keys, rotation=45); plt.tight_layout(); plt.savefig(hm_dir / 'rsa_layers_heatmap.png', dpi=150); plt.close()
                    with open(csv_root / f'rsa_layers_{p.stem}.csv', 'w', newline='', encoding='utf-8') as f:
                        w = csv.writer(f); w.writerow(['layer','spearman_rho']); [w.writerow([lk, s]) for lk, s in zip(layer_keys, scores)]
                    print(f"[analyze] RSA layers for {p}: L={len(layer_keys)}")
                # CKA
                if args.cka:
                    # Single vectors
                    if t_vecs:
                        m = min(len(s_vecs), len(t_vecs))
                        if m >= 2:
                            cka = compute_linear_cka(s_vecs[:m], t_vecs[:m])
                            with open(csv_root / f'cka_{p.stem}.csv', 'w', newline='', encoding='utf-8') as f:
                                w = csv.writer(f); w.writerow(['file','samples','cka']); w.writerow([str(p), m, cka])
                            print(f"[analyze] CKA for {p}: {cka:.3f}")
                    # Per layer
                    if layers_map:
                        layer_keys = sorted(layers_map.keys(), key=lambda x: int(x))
                        scores = []
                        for lk in layer_keys:
                            lv = layers_map[lk]
                            m = min(len(s_vecs), len(lv))
                            if m < 2:
                                scores.append(float('nan'))
                                continue
                            scores.append(compute_linear_cka(s_vecs[:m], lv[:m]))
                        # Plot line chart
                        plot_dir = png_root / 'cka_layers' / p.stem
                        plot_dir.mkdir(parents=True, exist_ok=True)
                        plt.figure(figsize=(max(6, len(layer_keys)), 3)); plt.plot(range(len(layer_keys)), scores, marker='o'); plt.title('CKA vs layers'); plt.xticks(range(len(layer_keys)), layer_keys, rotation=45); plt.tight_layout(); plt.savefig(plot_dir / 'cka_vs_layers.png', dpi=150); plt.close()
                        with open(csv_root / f'cka_layers_{p.stem}.csv', 'w', newline='', encoding='utf-8') as f:
                            w = csv.writer(f); w.writerow(['layer','cka']); [w.writerow([lk, s]) for lk, s in zip(layer_keys, scores)]
                        print(f"[analyze] CKA layers for {p}: L={len(layer_keys)}")

                # Logistic probe (substrate vs transformer)
                if args.probe:
                    from sklearn.preprocessing import LabelEncoder
                    from sklearn.model_selection import StratifiedKFold
                    from sklearn.linear_model import LogisticRegression
                    from sklearn.metrics import accuracy_score
                    # Determine labels
                    labels = None
                    if args.labels_file and Path(args.labels_file).exists():
                        labels = [l.strip() for l in open(args.labels_file, 'r', encoding='utf-8') if l.strip()]
                    elif 'labels' in t_payload:
                        labels = list(t_payload['labels'])
                    if not labels:
                        print(f"[analyze] WARN: probe skipped for {p}: no labels available")
                    else:
                        # Prepare substrate and transformer samples
                        # Substrate vectors are 4D [coh, asm, bnd, vel]; transformer vectors can be single or per-layer
                        def run_probe(X, y):
                            y_enc = LabelEncoder().fit_transform(y)
                            skf = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
                            accs = []
                            for train_idx, test_idx in skf.split(X, y_enc):
                                clf = LogisticRegression(max_iter=1000)
                                clf.fit(np.asarray(X)[train_idx], y_enc[train_idx])
                                y_pred = clf.predict(np.asarray(X)[test_idx])
                                accs.append(accuracy_score(y_enc[test_idx], y_pred))
                            return float(np.mean(accs)), float(np.std(accs)), accs
                        # Align lengths
                        n_base = len(labels)
                        def trim_len(n):
                            return min(n, len(s_vecs), n_base)
                        # Aggregate results
                        probe_dir = png_root / 'probe' / p.stem
                        probe_dir.mkdir(parents=True, exist_ok=True)
                        csv_path = csv_root / f'probe_{p.stem}.csv'
                        # Substrate
                        n = trim_len(len(s_vecs))
                        sub_mean, sub_std, sub_accs = run_probe(s_vecs[:n], labels[:n])
                        # Transformer: single vectors
                        tf_mean, tf_std = None, None
                        tf_layer_scores = None
                        if t_vecs:
                            m = trim_len(len(t_vecs))
                            tf_mean, tf_std, tf_accs = run_probe(t_vecs[:m], labels[:m])
                        # Transformer: per-layer
                        tf_layer_scores = []
                        layer_keys_sorted = []
                        if layers_map:
                            layer_keys_sorted = sorted(layers_map.keys(), key=lambda x: int(x))
                            for lk in layer_keys_sorted:
                                lv = layers_map[lk]
                                m = trim_len(len(lv))
                                mean_k, std_k, _ = run_probe(lv[:m], labels[:m])
                                tf_layer_scores.append((lk, mean_k, std_k))
                        # Plot bar chart
                        names = ['substrate']
                        vals = [sub_mean]
                        errs = [sub_std]
                        if tf_mean is not None:
                            names.append('transformer')
                            vals.append(tf_mean)
                            errs.append(tf_std)
                        for lk, mean_k, std_k in tf_layer_scores or []:
                            names.append(f'layer_{lk}')
                            vals.append(mean_k)
                            errs.append(std_k)
                        plt.figure(figsize=(max(8, len(names)*0.9), 4))
                        x = np.arange(len(names))
                        plt.bar(x, vals, yerr=errs, capsize=3)
                        plt.xticks(x, names, rotation=45, ha='right')
                        plt.ylabel('Probe Accuracy')
                        plt.title('Logistic Probe Accuracy (5-fold CV)')
                        plt.tight_layout()
                        plt.savefig(probe_dir / 'probe_accuracy.png', dpi=150)
                        plt.close()
                        # Write CSV
                        with open(csv_path, 'w', newline='', encoding='utf-8') as f:
                            w = csv.writer(f)
                            w.writerow(['name','mean_acc','std_acc'])
                            for nm, v, e in zip(names, vals, errs):
                                w.writerow([nm, v, e])
                        print(f"[analyze] Probe written for {p}: mean substrate={sub_mean:.3f}")
        except Exception as e:
            print(f"[analyze] WARN: analysis failed: {e}")

if __name__ == '__main__':
    main()

