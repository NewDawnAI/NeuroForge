#!/usr/bin/env python3
"""
Multi-run synapse snapshot analyzer for NeuroForge

Features
- Accepts one or multiple CSV snapshots (pre_neuron, post_neuron, weight)
- Computes per-run:
  - Total edges
  - Top-K neurons by in-degree and out-degree
  - Weight statistics (count, mean, std, min, p10, p50, p90, max)
- Single-run: generates a weight histogram SVG; optional GraphML export
- Multi-run: generates overlaid weight histograms SVG; optional summary CSV

Example usage (Windows PowerShell)
- Single run with histogram and GraphML:
  .\.venv\Scripts\python.exe scripts\analyze_synapses_phase_a.py \
      --input "build\\final_synapses.csv" \
      --histogram-out "build\\final_weights_hist.svg" \
      --graphml "build\\final_synapses.graphml"

- Multi-run with labels and overlay histogram + summary CSV:
  .\.venv\Scripts\python.exe scripts\analyze_synapses_phase_a.py \
      --input "build\\final_synapses_on.csv::ON" \
      --input "build\\final_synapses_ctrl.csv::CTRL" \
      --histogram-out "build\\weights_overlay_on_vs_ctrl.svg" \
      --summary-csv "build\\multi_run_summary.csv"
"""
from __future__ import annotations

import argparse
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple, Dict

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from itertools import combinations

try:
    import networkx as nx  # optional, used when --graphml/metrics/clustering are enabled
except Exception:  # pragma: no cover
    nx = None  # type: ignore

try:
    import scipy.sparse as sp  # type: ignore
except Exception:  # pragma: no cover
    sp = None  # type: ignore

# Add optional SciPy stats import for inferential tests
try:
    from scipy import stats as scipy_stats  # type: ignore
except Exception:  # pragma: no cover
    scipy_stats = None  # type: ignore

REQUIRED_COLUMNS = ["pre_neuron", "post_neuron", "weight"]


@dataclass
class RunData:
    label: str
    path: Path
    df: pd.DataFrame


def parse_input_spec(spec: str) -> Tuple[Path, str]:
    """
    Parse an input spec of the form:
      - path
      - path::label (double-colon to avoid conflict with Windows drive letters)
    Returns (path, label). If label omitted, derive from filename stem.
    """
    if "::" in spec:
        path_str, label = spec.split("::", 1)
    else:
        path_str, label = spec, None
    p = Path(path_str)
    if not label:
        label = p.stem
    return p, label


def load_edges_csv(p: Path) -> pd.DataFrame:
    if not p.exists():
        sys.stderr.write(f"ERROR: File not found: {p}\n")
        sys.exit(2)
    df = pd.read_csv(p)
    missing = [c for c in REQUIRED_COLUMNS if c not in df.columns]
    if missing:
        sys.stderr.write(f"ERROR: Missing required columns {missing} in {p}\n")
        sys.exit(2)
    # ensure correct dtypes
    df = df.copy()
    # best-effort coercion
    for col in ["pre_neuron", "post_neuron"]:
        df[col] = pd.to_numeric(df[col], errors="coerce").astype(pd.Int64Dtype())
    df["weight"] = pd.to_numeric(df["weight"], errors="coerce")
    # drop rows with NaNs
    df = df.dropna(subset=["pre_neuron", "post_neuron", "weight"]).astype({"pre_neuron": int, "post_neuron": int})
    return df


def compute_weight_stats(df: pd.DataFrame) -> Dict[str, float]:
    w = df["weight"].values.astype(float)
    return {
        "count": int(w.size),
        "mean": float(np.mean(w)) if w.size else float("nan"),
        "std": float(np.std(w, ddof=1)) if w.size > 1 else float("nan"),
        "min": float(np.min(w)) if w.size else float("nan"),
        "p10": float(np.percentile(w, 10)) if w.size else float("nan"),
        "p50": float(np.percentile(w, 50)) if w.size else float("nan"),
        "p90": float(np.percentile(w, 90)) if w.size else float("nan"),
        "max": float(np.max(w)) if w.size else float("nan"),
    }


def compute_degrees(df: pd.DataFrame) -> Tuple[pd.Series, pd.Series]:
    out_deg = df.groupby("pre_neuron").size().sort_values(ascending=False)
    in_deg = df.groupby("post_neuron").size().sort_values(ascending=False)
    return in_deg, out_deg


def print_run_report(label: str, df: pd.DataFrame, topk: int) -> Dict[str, float]:
    stats = compute_weight_stats(df)
    in_deg, out_deg = compute_degrees(df)

    print(f"\n=== Run: {label} ===")
    print(f"Total edges: {len(df)}")
    print("Weight stats:")
    print(
        f"  count={stats['count']} mean={stats['mean']:.6f} std={stats['std']:.6f} min={stats['min']:.6f} "
        f"p10={stats['p10']:.6f} p50={stats['p50']:.6f} p90={stats['p90']:.6f} max={stats['max']:.6f}"
    )

    def fmt_top(series: pd.Series, name: str):
        print(f"Top {topk} by {name}:")
        for nid, deg in series.head(topk).items():
            print(f"  {int(nid)}: {int(deg)}")

    fmt_top(in_deg, "in-degree")
    fmt_top(out_deg, "out-degree")
    return stats


def save_single_histogram(df: pd.DataFrame, out_path: Path, title: Optional[str] = None) -> None:
    weights = df["weight"].values.astype(float)
    plt.figure(figsize=(7, 4.5), dpi=150)
    plt.hist(weights, bins=30, alpha=0.8, color="#4C78A8", edgecolor="black")
    plt.xlabel("Synapse weight")
    plt.ylabel("Count")
    if title:
        plt.title(title)
    plt.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, format="svg")
    plt.close()


def save_overlay_histogram(weight_series: List[Tuple[str, np.ndarray]], out_path: Path, title: Optional[str] = None) -> None:
    if not weight_series:
        return
    # Determine global bins for comparability
    all_w = np.concatenate([w for _, w in weight_series]) if len(weight_series) > 1 else weight_series[0][1]
    w_min, w_max = float(np.min(all_w)), float(np.max(all_w))
    if w_min == w_max:
        w_min -= 1e-6
        w_max += 1e-6
    bins = np.linspace(w_min, w_max, 31)

    plt.figure(figsize=(8, 5), dpi=150)
    for label, w in weight_series:
        plt.hist(w, bins=bins, alpha=0.5, density=False, histtype="stepfilled", linewidth=1.0, label=label)
    plt.xlabel("Synapse weight")
    plt.ylabel("Count")
    if title:
        plt.title(title)
    plt.legend()
    plt.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, format="svg")
    plt.close()


def write_summary_csv(path: Path, rows: List[Dict[str, float]]) -> None:
    df = pd.DataFrame(rows)
    path.parent.mkdir(parents=True, exist_ok=True)
    df.to_csv(path, index=False)


def maybe_export_graphml(df: pd.DataFrame, out_path: Path) -> None:
    if nx is None:
        print("networkx not available; skipping GraphML export.")
        return
    G = nx.from_pandas_edgelist(df, "pre_neuron", "post_neuron", edge_attr="weight", create_using=nx.DiGraph())
    out_path.parent.mkdir(parents=True, exist_ok=True)
    nx.write_graphml(G, out_path)
    print(f"GraphML written: {out_path}")

# New: compute network metrics per run (directed variants)
def compute_network_metrics(df: pd.DataFrame) -> Dict[str, float]:
    metrics: Dict[str, float] = {}
    if nx is None:
        print("networkx not available; skipping network metrics.")
        return metrics
    G = nx.from_pandas_edgelist(df, "pre_neuron", "post_neuron", edge_attr="weight", create_using=nx.DiGraph())
    # Average clustering on undirected projection (weighted)
    try:
        avg_clust = nx.average_clustering(G.to_undirected(), weight="weight")
    except Exception:
        avg_clust = float("nan")
    metrics["avg_clustering"] = float(avg_clust)
    # Total-degree Pearson assortativity (legacy)
    try:
        assort_total = nx.degree_pearson_correlation_coefficient(G)
    except Exception:
        assort_total = float("nan")
    metrics["assortativity_total"] = float(assort_total)
    # Directed in/out variants (NetworkX supports x/y in {"in","out"} for directed graphs)
    for (x_dir, y_dir, key) in [
        ("in", "in", "assortativity_in"),
        ("out", "out", "assortativity_out"),
        ("in", "out", "assortativity_in_out"),
        ("out", "in", "assortativity_out_in"),
    ]:
        try:
            val = nx.degree_pearson_correlation_coefficient(G, x=x_dir, y=y_dir)
        except Exception:
            val = float("nan")
        metrics[key] = float(val)

    # Components
    try:
        wccs = list(nx.weakly_connected_components(G))
        sccs = list(nx.strongly_connected_components(G))
        n_wcc = len(wccs)
        n_scc = len(sccs)
        largest_wcc = max((len(c) for c in wccs), default=0)
        largest_scc = max((len(c) for c in sccs), default=0)
    except Exception:
        n_wcc = n_scc = largest_wcc = largest_scc = 0
    metrics["num_wcc"] = float(n_wcc)
    metrics["num_scc"] = float(n_scc)
    metrics["largest_wcc"] = float(largest_wcc)
    metrics["largest_scc"] = float(largest_scc)

    # Average shortest path length on largest WCC (unweighted)
    try:
        if n_wcc > 0:
            lwcc_nodes = max(wccs, key=len)
            H = G.subgraph(lwcc_nodes).to_undirected()
            aspl = nx.average_shortest_path_length(H)
        else:
            aspl = float("nan")
    except Exception:
        aspl = float("nan")
    metrics["avg_shortest_path_len_lwcc"] = float(aspl)
    return metrics

# New: export adjacency matrix and node list (dense)
def export_adjacency(df: pd.DataFrame, out_prefix: Path, label: str) -> None:
    if nx is None:
        print("networkx not available; skipping adjacency export.")
        return
    G = nx.from_pandas_edgelist(df, "pre_neuron", "post_neuron", edge_attr="weight", create_using=nx.DiGraph())
    nodes = sorted(set(df["pre_neuron"]).union(set(df["post_neuron"])))
    A = nx.to_numpy_array(G, nodelist=nodes, weight="weight", dtype=float)
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    mat_path = out_prefix.with_name(f"{out_prefix.name}_{label}_adj_matrix.csv")
    nodes_path = out_prefix.with_name(f"{out_prefix.name}_{label}_nodes.csv")
    # Write matrix and node list
    pd.DataFrame(A, index=nodes, columns=nodes).to_csv(mat_path, index=True)
    pd.Series(nodes, name="neuron_id").to_csv(nodes_path, index=False)
    print(f"Adjacency written: {mat_path} and nodes: {nodes_path}")

# New: export sparse adjacency (COO triples) per run
def export_sparse_adjacency(df: pd.DataFrame, out_prefix: Path, label: str) -> None:
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    coo_path = out_prefix.with_name(f"{out_prefix.name}_{label}_adj_coo.csv")
    # Ensure correct columns and types
    triples = df[["pre_neuron", "post_neuron", "weight"]].copy()
    triples.columns = ["pre", "post", "weight"]
    triples.to_csv(coo_path, index=False)
    print(f"Sparse adjacency (COO) written: {coo_path}")

# New: export sparse adjacency (NPZ) per run
def export_sparse_adjacency_npz(df: pd.DataFrame, out_prefix: Path, label: str) -> None:
    """
    Export sparse adjacency as NPZ (COO). If SciPy is available, writes a .npz via scipy.sparse.save_npz.
    Otherwise, falls back to numpy .npz with arrays: row, col, data, shape, nodes (node ids ordered by index).
    """
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    nodes = pd.Index(sorted(pd.unique(pd.concat([df["pre_neuron"], df["post_neuron"]], ignore_index=True))))
    node_to_idx = {int(n): i for i, n in enumerate(nodes)}
    rows = df["pre_neuron"].map(lambda n: node_to_idx[int(n)]).astype(int).to_numpy()
    cols = df["post_neuron"].map(lambda n: node_to_idx[int(n)]).astype(int).to_numpy()
    data = df["weight"].astype(float).to_numpy()
    shape = (len(nodes), len(nodes))
    base = out_prefix.with_name(f"{out_prefix.name}_{label}_adj_coo")
    npz_path = base.with_suffix(".npz")
    nodes_path = out_prefix.with_name(f"{out_prefix.name}_{label}_nodes.csv")
    # Write node list mapping
    pd.DataFrame({"node": nodes.astype(int), "index": range(len(nodes))}).to_csv(nodes_path, index=False)
    if sp is not None:
        try:
            mat = sp.coo_matrix((data, (rows, cols)), shape=shape)
            sp.save_npz(npz_path, mat)
            print(f"Sparse adjacency (NPZ SciPy) written: {npz_path} and nodes: {nodes_path}")
            return
        except Exception:
            pass
    # Fallback: numpy savez
    np.savez_compressed(npz_path, row=rows, col=cols, data=data, shape=np.array(shape, dtype=np.int64))
    print(f"Sparse adjacency (NPZ NumPy) written: {npz_path} and nodes: {nodes_path}")

# New: ECDF/QQ plotting and cross-run delta helpers (moved above main)
# (ensure clustering helpers exist as they are used by delta and optional outputs)

def _ecdf(x: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    x = np.sort(x)
    n = x.size
    y = np.arange(1, n + 1) / n
    return x, y


# Add: per-node clustering coefficient computation (undirected projection, weighted if available)

def compute_clustering_coefficients(df: pd.DataFrame) -> List[Tuple[int, float]]:
    if nx is None:
        return []
    try:
        G = nx.from_pandas_edgelist(df, "pre_neuron", "post_neuron", edge_attr="weight", create_using=nx.DiGraph())
        GU = G.to_undirected()
        try:
            cdict = nx.clustering(GU, weight="weight")
        except Exception:
            cdict = nx.clustering(GU)
        rows: List[Tuple[int, float]] = [(int(n), float(v)) for n, v in cdict.items()]
        rows.sort(key=lambda t: t[0])
        return rows
    except Exception:
        return []


def save_ecdf_overlay(series: List[Tuple[str, np.ndarray]], out_path: Path, title: str = "ECDF (overlay)") -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.figure(figsize=(8, 5))
    for label, arr in series:
        xs, ys = _ecdf(arr)
        plt.step(xs, ys, where="post", label=label)
    plt.xlabel("Weight")
    plt.ylabel("ECDF")
    plt.title(title)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, format="svg")
    plt.close()


def save_qq_plot(a: np.ndarray, b: np.ndarray, label_a: str, label_b: str, out_path: Path, title: str = "QQ plot") -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    q = np.linspace(0.01, 0.99, 99)
    qa = np.quantile(a, q)
    qb = np.quantile(b, q)
    plt.figure(figsize=(6, 6))
    plt.scatter(qa, qb, s=10, alpha=0.7)
    lo = min(qa.min(), qb.min())
    hi = max(qa.max(), qb.max())
    plt.plot([lo, hi], [lo, hi], 'k--', linewidth=1)
    plt.xlabel(f"Quantiles of {label_a}")
    plt.ylabel(f"Quantiles of {label_b}")
    plt.title(title)
    plt.tight_layout()
    plt.savefig(out_path, format="svg")
    plt.close()


# Add: degree and clustering histogram helpers referenced by optional outputs

def save_degree_histograms(df: pd.DataFrame, out_prefix: Path, label: str) -> None:
    in_deg, out_deg = compute_degrees(df)
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    in_path = out_prefix.with_name(f"{out_prefix.name}_{label}_in_degree_hist.svg")
    out_path = out_prefix.with_name(f"{out_prefix.name}_{label}_out_degree_hist.svg")
    plt.figure(figsize=(7, 4.5), dpi=150)
    plt.hist(in_deg.values.astype(float), bins=30, color="#72B7B2", edgecolor="black", alpha=0.85)
    plt.xlabel("In-degree")
    plt.ylabel("Count")
    plt.title(f"In-degree distribution: {label}")
    plt.tight_layout()
    plt.savefig(in_path, format="svg")
    plt.close()

    plt.figure(figsize=(7, 4.5), dpi=150)
    plt.hist(out_deg.values.astype(float), bins=30, color="#E45756", edgecolor="black", alpha=0.85)
    plt.xlabel("Out-degree")
    plt.ylabel("Count")
    plt.title(f"Out-degree distribution: {label}")
    plt.tight_layout()
    plt.savefig(out_path, format="svg")
    plt.close()


def save_clustering_histogram(df: pd.DataFrame, out_prefix: Path, label: str) -> None:
    rows = compute_clustering_coefficients(df)
    if not rows:
        print("Clustering not available; skipping clustering histogram.")
        return
    coeffs = np.array([v for _, v in rows], dtype=float)
    coeffs = coeffs[np.isfinite(coeffs)]
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    out_path = out_prefix.with_name(f"{out_prefix.name}_{label}_clustering_hist.svg")
    plt.figure(figsize=(7, 4.5), dpi=150)
    plt.hist(coeffs, bins=30, color="#54A24B", edgecolor="black", alpha=0.85)
    plt.xlabel("Clustering coefficient")
    plt.ylabel("Count")
    plt.title(f"Clustering coefficients: {label}")
    plt.tight_layout()
    plt.savefig(out_path, format="svg")
    plt.close()

def _edge_set(df: pd.DataFrame) -> set[tuple[int, int]]:
    return set(zip(df["pre_neuron"].astype(int).to_list(), df["post_neuron"].astype(int).to_list()))


def _series_from_clustering(df: pd.DataFrame) -> pd.Series:
    rows = compute_clustering_coefficients(df)
    return pd.Series({nid: coef for nid, coef in rows}, dtype=float)


def _bh_fdr_adjust(pvals: List[float]) -> List[float]:
    if not pvals:
        return []
    arr = np.asarray(pvals, dtype=float)
    m = arr.size
    order = np.argsort(arr)
    sorted_p = arr[order]
    adjusted_sorted = np.empty(m, dtype=float)
    prev = 1.0
    for i in range(m - 1, -1, -1):
        rank = i + 1
        val = (sorted_p[i] * m) / rank
        if val < prev:
            prev = val
        adjusted_sorted[i] = prev
    adjusted = np.empty(m, dtype=float)
    adjusted[order] = np.minimum(adjusted_sorted, 1.0)
    return adjusted.tolist()


def _cliffs_delta_from_U(U: float, n1: int, n2: int) -> float:
    if n1 <= 0 or n2 <= 0:
        return float("nan")
    return (2.0 * float(U) / float(n1 * n2)) - 1.0


def do_cross_run_delta(runs: List[RunData], weight_series: List[Tuple[str, np.ndarray]], out_prefix: Path, topk: int = 20, mcorrect: str = "bonferroni", permute_overlap: int = 0, permute_seed: int = 42) -> None:
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    edges_rows: List[Dict[str, object]] = []
    weight_rows: List[Dict[str, object]] = []
    summary_lines: List[str] = [
        "Cross-run Delta Summary",
        "=======================",
    ]
    wmap = {label: arr for (label, arr) in weight_series}
    for (run_a, run_b) in combinations(runs, 2):
        la, lb = run_a.label, run_b.label
        df_a, df_b = run_a.df, run_b.df
        Ea, Eb = _edge_set(df_a), _edge_set(df_b)
        inter = len(Ea & Eb)
        union = len(Ea | Eb)
        jacc = (inter / union) if union > 0 else float("nan")
        edges_rows.append({
            "label_a": la, "label_b": lb,
            "edges_a": len(Ea), "edges_b": len(Eb),
            "intersection": inter, "union": union, "jaccard": jacc,
        })
        wa, wb = wmap[la], wmap[lb]
        def stats(x: np.ndarray) -> Dict[str, float]:
            return {
                "mean": float(np.mean(x)),
                "std": float(np.std(x, ddof=0)),
                "min": float(np.min(x)),
                "p10": float(np.quantile(x, 0.10)),
                "p50": float(np.quantile(x, 0.50)),
                "p90": float(np.quantile(x, 0.90)),
                "max": float(np.max(x)),
            }
        sa, sb = stats(wa), stats(wb)
        for k in ["mean", "std", "min", "p10", "p50", "p90", "max"]:
            weight_rows.append({
                "metric": k, "label_a": la, "value_a": sa[k], "label_b": lb, "value_b": sb[k], "delta_b_minus_a": float(sb[k] - sa[k])
            })
        in_a, out_a = compute_degrees(df_a)
        in_b, out_b = compute_degrees(df_b)
        all_nodes = sorted(set(map(int, in_a.index)) | set(map(int, out_a.index)) | set(map(int, in_b.index)) | set(map(int, out_b.index)))
        s_in_a = pd.Series(in_a, dtype=float, index=in_a.index).reindex(all_nodes, fill_value=0.0)
        s_in_b = pd.Series(in_b, dtype=float, index=in_b.index).reindex(all_nodes, fill_value=0.0)
        s_out_a = pd.Series(out_a, dtype=float, index=out_a.index).reindex(all_nodes, fill_value=0.0)
        s_out_b = pd.Series(out_b, dtype=float, index=out_b.index).reindex(all_nodes, fill_value=0.0)
        c_a = _series_from_clustering(df_a).reindex(all_nodes, fill_value=np.nan)
        c_b = _series_from_clustering(df_b).reindex(all_nodes, fill_value=np.nan)
        node_df = pd.DataFrame({
            "neuron_id": all_nodes,
            f"in_{la}": s_in_a.values,
            f"in_{lb}": s_in_b.values,
            "delta_in": (s_in_b - s_in_a).values,
            f"out_{la}": s_out_a.values,
            f"out_{lb}": s_out_b.values,
            "delta_out": (s_out_b - s_out_a).values,
            f"clust_{la}": c_a.values,
            f"clust_{lb}": c_b.values,
            "delta_clust": (c_b - c_a).values,
        })
        pair_base = out_prefix.with_name(f"{out_prefix.name}_{la}_vs_{lb}")
        node_out = pair_base.with_name(f"{pair_base.name}_node_deltas.csv")
        node_df.to_csv(node_out, index=False)
        def topk_rows(df: pd.DataFrame, col: str) -> List[Tuple[int, float]]:
            s = df[["neuron_id", col]].dropna()
            s["abs"] = s[col].abs()
            s = s.sort_values("abs", ascending=False).head(topk)
            return [(int(r["neuron_id"]), float(r[col])) for _, r in s.iterrows()]
        top_in = topk_rows(node_df, "delta_in")
        top_out = topk_rows(node_df, "delta_out")
        top_cl = topk_rows(node_df, "delta_clust")
        summary_lines.append("")
        summary_lines.append(f"Pair {la} vs {lb}")
        summary_lines.append("-----------------")
        summary_lines.append(f"Edges: |A|={len(Ea)} |B|={len(Eb)} Intersection={inter} Union={union} Jaccard={jacc:.4f}")
        summary_lines.append("Top-{} |Δ in-degree|:".format(topk))
        for nid, v in top_in:
            summary_lines.append(f"  neuron {nid}: {v:+.3f}")
        summary_lines.append("Top-{} |Δ out-degree|:".format(topk))
        for nid, v in top_out:
            summary_lines.append(f"  neuron {nid}: {v:+.3f}")
        summary_lines.append("Top-{} |Δ clustering|:".format(topk))
        for nid, v in top_cl:
            summary_lines.append(f"  neuron {nid}: {v:+.3f}")
        save_overlay_histogram([(la, wa), (lb, wb)], pair_base.with_name(f"{pair_base.name}_hist.svg"), title=f"Weights: {la} vs {lb}")
        save_ecdf_overlay([(la, wa), (lb, wb)], pair_base.with_name(f"{pair_base.name}_ecdf.svg"), title=f"ECDF: {la} vs {lb}")
        save_qq_plot(wa, wb, la, lb, pair_base.with_name(f"{pair_base.name}_qq.svg"), title=f"QQ: {la} vs {lb}")
        # Statistical tests: report raw, Bonferroni, and FDR; include effect sizes
        summary_lines.append("")
        if scipy_stats is None:
            summary_lines.append("[Note] SciPy not available; statistical tests (KS, Mann–Whitney, Fisher exact) skipped.")
        else:
            alpha = 0.05
            tests_performed: List[Tuple[str, float, str]] = []  # (name, pvalue, detail)
            # Clean arrays
            wA = np.asarray(wa, dtype=float)
            wB = np.asarray(wb, dtype=float)
            wA = wA[np.isfinite(wA)]
            wB = wB[np.isfinite(wB)]
            inA = np.asarray(s_in_a.values, dtype=float)
            inB = np.asarray(s_in_b.values, dtype=float)
            outA = np.asarray(s_out_a.values, dtype=float)
            outB = np.asarray(s_out_b.values, dtype=float)
            clA = np.asarray(c_a.values, dtype=float)
            clB = np.asarray(c_b.values, dtype=float)
            cl_mask = np.isfinite(clA) & np.isfinite(clB)
            clA = clA[cl_mask]
            clB = clB[cl_mask]
            def add_test(name: str, p: float, detail: str) -> None:
                tests_performed.append((name, p, detail))
            # Weights: KS and Mann–Whitney (include effect sizes: D, Cliff's delta)
            if wA.size > 0 and wB.size > 0:
                ks = scipy_stats.ks_2samp(wA, wB, alternative="two-sided", mode="auto")
                add_test("weights_KS", float(ks.pvalue), f"D={ks.statistic:.4g}")
                try:
                    mw = scipy_stats.mannwhitneyu(wA, wB, alternative="two-sided")
                    n1, n2 = int(wA.size), int(wB.size)
                    delta = _cliffs_delta_from_U(float(mw.statistic), n1, n2)
                    add_test("weights_MW", float(mw.pvalue), f"U={float(mw.statistic):.4g}, Cliff_delta={delta:.3f}")
                except Exception:
                    pass
            # In-degree
            if inA.size > 0 and inB.size > 0:
                ks_in = scipy_stats.ks_2samp(inA, inB, alternative="two-sided", mode="auto")
                add_test("in_degree_KS", float(ks_in.pvalue), f"D={ks_in.statistic:.4g}")
                try:
                    mw_in = scipy_stats.mannwhitneyu(inA, inB, alternative="two-sided")
                    delta_in = _cliffs_delta_from_U(float(mw_in.statistic), int(inA.size), int(inB.size))
                    add_test("in_degree_MW", float(mw_in.pvalue), f"U={float(mw_in.statistic):.4g}, Cliff_delta={delta_in:.3f}")
                except Exception:
                    pass
            # Out-degree
            if outA.size > 0 and outB.size > 0:
                ks_out = scipy_stats.ks_2samp(outA, outB, alternative="two-sided", mode="auto")
                add_test("out_degree_KS", float(ks_out.pvalue), f"D={ks_out.statistic:.4g}")
                try:
                    mw_out = scipy_stats.mannwhitneyu(outA, outB, alternative="two-sided")
                    delta_out = _cliffs_delta_from_U(float(mw_out.statistic), int(outA.size), int(outB.size))
                    add_test("out_degree_MW", float(mw_out.pvalue), f"U={float(mw_out.statistic):.4g}, Cliff_delta={delta_out:.3f}")
                except Exception:
                    pass
            # Clustering coefficients: Mann–Whitney + Cliff's delta
            if clA.size > 0 and clB.size > 0:
                try:
                    mw_cl = scipy_stats.mannwhitneyu(clA, clB, alternative="two-sided")
                    delta_cl = _cliffs_delta_from_U(float(mw_cl.statistic), int(clA.size), int(clB.size))
                    add_test("clustering_MW", float(mw_cl.pvalue), f"U={float(mw_cl.statistic):.4g}, Cliff_delta={delta_cl:.3f}")
                except Exception:
                    pass
            # Edge overlap vs chance: Fisher's exact (right-tail), report odds ratio
            try:
                n_nodes = len(all_nodes)
                U_total = max(n_nodes * (n_nodes - 1), 1)
                a_only = len(Ea) - inter
                b_only = len(Eb) - inter
                neither = U_total - union
                table = [[inter, a_only], [b_only, max(neither, 0)]]
                odds, p_fisher = scipy_stats.fisher_exact(table, alternative="greater")
                add_test("edge_overlap_Fisher", float(p_fisher), f"odds={float(odds):.4g}")
            except Exception:
                pass
            # Optional permutation test for edge overlap (preserve out-degree and target multiset via target shuffles)
            if permute_overlap and permute_overlap > 0:
                rng = np.random.default_rng(int(permute_seed))
                pre_b = df_b["pre_neuron"].astype(int).to_numpy()
                post_b = df_b["post_neuron"].astype(int).to_numpy()
                inter_samples = np.zeros(int(permute_overlap), dtype=float)
                for i in range(int(permute_overlap)):
                    perm = rng.permutation(post_b)
                    Eb_perm = set(zip(pre_b.tolist(), perm.tolist()))
                    inter_samples[i] = float(len(Ea & Eb_perm))
                mean_null = float(np.mean(inter_samples))
                sd_null = float(np.std(inter_samples, ddof=1)) if inter_samples.size > 1 else float("nan")
                z = float((inter - mean_null) / sd_null) if np.isfinite(sd_null) and sd_null > 0 else float("nan")
                # right-tail p-value with +1 smoothing
                p_perm = float((np.sum(inter_samples >= float(inter)) + 1.0) / (inter_samples.size + 1.0))
                add_test("edge_overlap_Permutation", p_perm, f"N={int(permute_overlap)}, z={z:.3f}, mean={mean_null:.2f}, sd={sd_null:.2f}")
            # Multiple testing corrections
            m = max(len(tests_performed), 1)
            summary_lines.append(f"Statistical tests (alpha=0.05; m={m}; primary correction={mcorrect}):")
            # Build maps for p-values
            pmap_raw: Dict[str, Tuple[float, str]] = {name: (p, detail) for name, p, detail in tests_performed}
            pvals = [p for _, p, _ in tests_performed]
            bonf = [float(min(1.0, p * m)) for p in pvals]
            fdr = _bh_fdr_adjust(pvals)
            name_order = [name for name, _, _ in tests_performed]
            pmap_bonf = {name_order[i]: bonf[i] for i in range(len(name_order))}
            pmap_fdr = {name_order[i]: fdr[i] for i in range(len(name_order))}
            def fmt_entry(name: str, label: str) -> Optional[str]:
                if name not in pmap_raw:
                    return None
                p, det = pmap_raw[name]
                return f"{label} {det}, p={p:.3g}, bonf={pmap_bonf.get(name, float('nan')):.3g}, fdr={pmap_fdr.get(name, float('nan')):.3g}"
            def is_sig(name: str) -> bool:
                if name not in pmap_raw:
                    return False
                if mcorrect == "fdr":
                    return pmap_fdr.get(name, 1.0) < alpha
                else:  # default Bonferroni
                    return pmap_bonf.get(name, 1.0) < alpha
            # Weights narrative
            if ("weights_KS" in pmap_raw) or ("weights_MW" in pmap_raw):
                parts: List[str] = []
                if "weights_KS" in pmap_raw:
                    s = fmt_entry("weights_KS", "KS")
                    if s:
                        parts.append(s)
                if "weights_MW" in pmap_raw:
                    s = fmt_entry("weights_MW", "Mann–Whitney")
                    if s:
                        parts.append(s)
                if "weights_KS" in pmap_raw and "weights_MW" in pmap_raw:
                    both_sig = is_sig("weights_KS") and is_sig("weights_MW")
                    verdict = f"both < 0.05 after {mcorrect}" if both_sig else f"not both < 0.05 after {mcorrect}"
                    summary_lines.append("Weight distributions: " + "; ".join(parts) + f" — {verdict}.")
                else:
                    summary_lines.append("Weight distributions: " + "; ".join(parts))
            # In-degree
            if ("in_degree_KS" in pmap_raw) or ("in_degree_MW" in pmap_raw):
                parts: List[str] = []
                if "in_degree_KS" in pmap_raw:
                    s = fmt_entry("in_degree_KS", "KS")
                    if s:
                        parts.append(s)
                if "in_degree_MW" in pmap_raw:
                    s = fmt_entry("in_degree_MW", "Mann–Whitney")
                    if s:
                        parts.append(s)
                summary_lines.append("In-degree: " + "; ".join(parts))
            # Out-degree
            if ("out_degree_KS" in pmap_raw) or ("out_degree_MW" in pmap_raw):
                parts: List[str] = []
                if "out_degree_KS" in pmap_raw:
                    s = fmt_entry("out_degree_KS", "KS")
                    if s:
                        parts.append(s)
                if "out_degree_MW" in pmap_raw:
                    s = fmt_entry("out_degree_MW", "Mann–Whitney")
                    if s:
                        parts.append(s)
                summary_lines.append("Out-degree: " + "; ".join(parts))
            # Clustering
            if "clustering_MW" in pmap_raw:
                s = fmt_entry("clustering_MW", "Clustering Mann–Whitney")
                if s:
                    summary_lines.append(s)
            # Edge overlap Fisher
            if "edge_overlap_Fisher" in pmap_raw:
                s = fmt_entry("edge_overlap_Fisher", "Edge overlap Fisher")
                if s:
                    summary_lines.append(s)
            # Edge overlap permutation (if run)
            if "edge_overlap_Permutation" in pmap_raw:
                s = fmt_entry("edge_overlap_Permutation", "Edge overlap Permutation")
                if s:
                    summary_lines.append(s)
    edges_csv = out_prefix.with_name(f"{out_prefix.name}_edges_overlap.csv")
    weight_csv = out_prefix.with_name(f"{out_prefix.name}_weight_shift.csv")
    pd.DataFrame(edges_rows).to_csv(edges_csv, index=False)
    pd.DataFrame(weight_rows).to_csv(weight_csv, index=False)
    summary_txt = out_prefix.with_name(f"{out_prefix.name}_summary.txt")
    with open(summary_txt, "w", encoding="utf-8") as f:
        f.write("\n".join(summary_lines) + "\n")
    print(f"Delta edges CSV written: {edges_csv}")
    print(f"Delta weights CSV written: {weight_csv}")
    print(f"Delta summary written: {summary_txt}")

def main() -> None:
    ap = argparse.ArgumentParser(description="Analyze synapse CSVs: per-run metrics, plots, and cross-run deltas.")
    ap.add_argument("--input", action="append", metavar="PATH::LABEL", help="Edge CSV with 'pre_neuron,post_neuron,weight' and a label, e.g. data/run.csv::CTRL (use :: to avoid conflict with Windows drive letters)", required=True)
    ap.add_argument("--histogram-out", type=str, default=None, help="Path to output SVG histogram (single or overlay).")
    ap.add_argument("--summary-csv", type=str, default=None, help="Path to write per-run summary CSV (multi-run only).")
    ap.add_argument("--graphml", type=str, default=None, help="Single-run only: write GraphML to this path.")
    ap.add_argument("--topk", type=int, default=5, help="Top-K neurons by degree to display.")
    # New CLI flags
    ap.add_argument("--metrics-csv", type=str, default=None, help="Optional: write per-run network metrics to this CSV.")
    ap.add_argument("--adjacency-prefix", type=str, default=None, help="Optional: prefix for adjacency CSV export per run.")
    ap.add_argument("--adjacency-sparse-prefix", type=str, default=None, help="Optional: prefix for sparse adjacency (COO triples) CSV export per run.")
    ap.add_argument("--adjacency-sparse-npz-prefix", type=str, default=None, help="Optional: prefix for sparse adjacency NPZ export per run (COO)")
    ap.add_argument("--delta-prefix", type=str, default=None, help="Optional: base prefix for cross-run delta outputs (CSV + plots + summary), multi-run only")
    ap.add_argument("--delta-topk", type=int, default=20, help="Top-K for node-level delta summaries (default: 20)")
    ap.add_argument("--degree-plots-prefix", type=str, default=None, help="Optional: prefix for in/out-degree histogram SVGs per run.")
    ap.add_argument("--clustering-plots-prefix", type=str, default=None, help="Optional: prefix for clustering histogram SVGs per run.")
    ap.add_argument("--clustering-csv", type=str, default=None, help="Optional: write per-node clustering coefficients CSV (includes label in multi-run).")
    # Multiple testing correction and permutation options for cross-run deltas
    ap.add_argument("--mcorrect", choices=["bonferroni", "fdr"], default="bonferroni", help="Primary multiple-testing correction used to mark significance (both Bonferroni and FDR values are reported).")
    ap.add_argument("--delta-permute-overlap", type=int, default=0, help="If >0, run this many permutations to assess edge-overlap significance (null via shuffling targets in B; preserves out-degree multiset). 0 disables.")
    ap.add_argument("--delta-permute-seed", type=int, default=42, help="RNG seed for permutation test of edge overlap.")

    args = ap.parse_args()

    inputs: List[Tuple[Path, str]] = [parse_input_spec(spec) for spec in args.input]
    # Helper to maybe run optional per-run outputs
    def optional_outputs(label: str, df: pd.DataFrame, metrics_rows: List[Dict[str, float]], clustering_rows: List[Dict[str, object]]):
        # metrics
        if args.metrics_csv is not None:
            m = compute_network_metrics(df)
            metrics_rows.append({"label": label, **m})
        # adjacency (dense)
        if args.adjacency_prefix is not None:
            export_adjacency(df, Path(args.adjacency_prefix), label)
        # adjacency (sparse COO)
        if args.adjacency_sparse_prefix is not None:
            export_sparse_adjacency(df, Path(args.adjacency_sparse_prefix), label)
        # adjacency (sparse NPZ)
        if args.adjacency_sparse_npz_prefix is not None:
            export_sparse_adjacency_npz(df, Path(args.adjacency_sparse_npz_prefix), label)
        # degree plots
        if args.degree_plots_prefix is not None:
            save_degree_histograms(df, Path(args.degree_plots_prefix), label)
        # clustering plots
        if args.clustering_plots_prefix is not None:
            save_clustering_histogram(df, Path(args.clustering_plots_prefix), label)
        # clustering CSV (per-node)
        if args.clustering_csv is not None:
            rows = compute_clustering_coefficients(df)
            for nid, coef in rows:
                clustering_rows.append({"label": label, "neuron_id": nid, "clustering_coef": coef})

    if len(inputs) == 1:
        # Single-run mode
        path, label = inputs[0]
        df = load_edges_csv(path)
        stats = print_run_report(label, df, topk=args.topk)
        if args.histogram_out:
            save_single_histogram(df, Path(args.histogram_out), title=f"Weights: {label}")
            print(f"Histogram SVG written: {args.histogram_out}")
        if args.graphml:
            maybe_export_graphml(df, Path(args.graphml))
        # Optional single-run extras
        metrics_rows: List[Dict[str, float]] = []
        clustering_rows: List[Dict[str, object]] = []
        optional_outputs(label, df, metrics_rows, clustering_rows)
        if args.metrics_csv and metrics_rows:
            write_summary_csv(Path(args.metrics_csv), metrics_rows)
            print(f"Metrics CSV written: {args.metrics_csv}")
        if args.clustering_csv and clustering_rows:
            outp = Path(args.clustering_csv)
            outp.parent.mkdir(parents=True, exist_ok=True)
            pd.DataFrame(clustering_rows).to_csv(outp, index=False)
            print(f"Clustering CSV written: {args.clustering_csv}")
        # echo basic summary for programmatic use
        print("\nSummary:")
        print({"label": label, **stats})
        return

    # Multi-run mode
    runs: List[RunData] = []
    for p, label in inputs:
        df = load_edges_csv(p)
        runs.append(RunData(label=label, path=p, df=df))

    summaries: List[Dict[str, float]] = []
    weight_series: List[Tuple[str, np.ndarray]] = []
    metrics_rows: List[Dict[str, float]] = []
    clustering_rows: List[Dict[str, object]] = []

    for run in runs:
        stats = print_run_report(run.label, run.df, topk=args.topk)
        summaries.append({"label": run.label, "edges": len(run.df), **stats})
        weight_series.append((run.label, run.df["weight"].values.astype(float)))
        optional_outputs(run.label, run.df, metrics_rows, clustering_rows)

    if args.histogram_out:
        save_overlay_histogram(weight_series, Path(args.histogram_out), title="Weights (overlay)")
        print(f"Overlay histogram SVG written: {args.histogram_out}")
    else:
        print("Note: --histogram-out not provided; overlay histogram not generated.")

    if args.summary_csv:
        write_summary_csv(Path(args.summary_csv), summaries)
        print(f"Summary CSV written: {args.summary_csv}")

    if args.metrics_csv and metrics_rows:
        write_summary_csv(Path(args.metrics_csv), metrics_rows)
        print(f"Metrics CSV written: {args.metrics_csv}")

    if args.clustering_csv and clustering_rows:
        outp = Path(args.clustering_csv)
        outp.parent.mkdir(parents=True, exist_ok=True)
        pd.DataFrame(clustering_rows).to_csv(outp, index=False)
        print(f"Clustering CSV written: {args.clustering_csv}")

    # Cross-run delta analysis
    if args.delta_prefix is not None and len(runs) >= 2:
        do_cross_run_delta(runs, weight_series, Path(args.delta_prefix), topk=args.delta_topk, mcorrect=args.mcorrect, permute_overlap=args.delta_permute_overlap, permute_seed=args.delta_permute_seed)

    if args.metrics_csv and metrics_rows:
        write_summary_csv(Path(args.metrics_csv), metrics_rows)
        print(f"Metrics CSV written: {args.metrics_csv}")

    if args.clustering_csv and clustering_rows:
        outp = Path(args.clustering_csv)
        outp.parent.mkdir(parents=True, exist_ok=True)
        pd.DataFrame(clustering_rows).to_csv(outp, index=False)
        print(f"Clustering CSV written: {args.clustering_csv}")

    print("\nCombined summary:")
    print(pd.DataFrame(summaries))


if __name__ == "__main__":
    main()