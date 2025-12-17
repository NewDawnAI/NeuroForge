#!/usr/bin/env python3
"""
Generate a Markdown production report for NeuroForge.

Collects latest CSV exports + integrity report, computes summary stats,
and embeds small base64 plots for inline visualization.

Example:
  python scripts/generate_production_report.py --exports build/production_exports --out build/production_exports/production_report.md
"""

import argparse
import base64
import io
import json
import os
from datetime import datetime
from typing import Any, Dict, List, Optional, Tuple

def try_imports():
    pd = np = plt = None
    try:
        import pandas as pd  # type: ignore
    except Exception:
        pd = None
    try:
        import numpy as np  # type: ignore
    except Exception:
        np = None
    try:
        import matplotlib.pyplot as plt  # type: ignore
    except Exception:
        plt = None
    return pd, np, plt

def read_csv_any(path: str, pd) -> Tuple[List[Dict[str, Any]], List[str]]:
    if pd is not None:
        df = pd.read_csv(path)
        return df.to_dict(orient="records"), list(df.columns)
    # fallback
    import csv
    rows: List[Dict[str, Any]] = []
    with open(path, 'r', newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append(r)
        return rows, reader.fieldnames or []

def to_float_safe(v: Any) -> Optional[float]:
    try:
        if v is None:
            return None
        if isinstance(v, (int, float)):
            return float(v)
        s = str(v).strip()
        if s == "" or s.lower() == "none":
            return None
        return float(s)
    except Exception:
        return None

def find_col(cands: List[str], cols: List[str]) -> Optional[str]:
    for c in cands:
        if c in cols:
            return c
    return None

def slope_linear(xs: List[float], ys: List[float], np_mod) -> Optional[float]:
    if len(xs) < 2 or len(xs) != len(ys):
        return None
    try:
        if np_mod is not None:
            m, b = np_mod.polyfit(xs, ys, 1)
            return float(m)
        # fallback simple slope
        x_mean = sum(xs) / len(xs)
        y_mean = sum(ys) / len(ys)
        num = sum((x - x_mean) * (y - y_mean) for x, y in zip(xs, ys))
        den = sum((x - x_mean) ** 2 for x in xs)
        if den == 0:
            return None
        return num / den
    except Exception:
        return None

def encode_plot_png(plt_mod, xs: List[float], ys: List[float], title: str, xlabel: str, ylabel: str) -> Optional[str]:
    if plt_mod is None or len(xs) == 0 or len(ys) == 0:
        return None
    fig, ax = plt_mod.subplots(figsize=(4, 2.5), dpi=120)
    ax.plot(xs, ys, linewidth=1.5)
    ax.set_title(title)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.grid(True, alpha=0.3)
    buf = io.BytesIO()
    fig.tight_layout()
    fig.savefig(buf, format='png')
    plt_mod.close(fig)
    data = base64.b64encode(buf.getvalue()).decode('ascii')
    return f"<img alt='{title}' src='data:image/png;base64,{data}' />"

def main():
    parser = argparse.ArgumentParser(description="Generate NeuroForge production Markdown report")
    parser.add_argument("--exports", required=True, help="Exports directory containing CSVs and integrity_report.json")
    parser.add_argument("--out", required=True, help="Output Markdown path")
    args = parser.parse_args()

    exports_dir = args.exports
    out_path = args.out
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    pd, np_mod, plt_mod = try_imports()

    import glob
    def pick_csv(bases: List[str]) -> Optional[str]:
        candidates: List[str] = []
        for pat in bases:
            candidates.extend(glob.glob(os.path.join(exports_dir, pat)))
        if not candidates:
            return None
        try:
            candidates.sort(key=lambda p: os.path.getmtime(p), reverse=True)
        except Exception:
            pass
        return candidates[0]

    ls_path = pick_csv(["learning_stats.csv", "learning_stats_run_*.csv"]) or os.path.join(exports_dir, "learning_stats.csv")
    rw_path = pick_csv(["reward_log.csv", "reward_log_run_*.csv"]) or os.path.join(exports_dir, "reward_log.csv")
    ir_path = os.path.join(exports_dir, "integrity_report.json")

    learning_rows: List[Dict[str, Any]] = []
    learning_cols: List[str] = []
    reward_rows: List[Dict[str, Any]] = []
    reward_cols: List[str] = []
    integrity: Dict[str, Any] = {}

    if ls_path and os.path.exists(ls_path):
        learning_rows, learning_cols = read_csv_any(ls_path, pd)
    if rw_path and os.path.exists(rw_path):
        reward_rows, reward_cols = read_csv_any(rw_path, pd)
    if os.path.exists(ir_path):
        try:
            with open(ir_path, 'r', encoding='utf-8') as f:
                integrity = json.load(f)
        except Exception:
            integrity = {"error": f"Failed to read {ir_path}"}

    # Extract stats from learning_stats
    hz_col = find_col(["processing_hz", "hz", "proc_hz"], learning_cols)
    upd_col = find_col(["updates", "update_count"], learning_cols)
    ts_col_ls = find_col(["timestamp_ms", "ts_ms", "ts", "time_ms", "time"], learning_cols)
    ls_hz = [to_float_safe(r.get(hz_col)) for r in learning_rows] if hz_col else []
    ls_hz = [v for v in ls_hz if v is not None]
    ls_upd = [to_float_safe(r.get(upd_col)) for r in learning_rows] if upd_col else []
    ls_upd = [v for v in ls_upd if v is not None]
    ls_ts = [to_float_safe(r.get(ts_col_ls)) for r in learning_rows] if ts_col_ls else []
    ls_ts = [v for v in ls_ts if v is not None]

    hz_mean = round(sum(ls_hz)/len(ls_hz), 3) if ls_hz else None
    hz_std = None
    if ls_hz and len(ls_hz) > 1:
        m = sum(ls_hz)/len(ls_hz)
        var = sum((x-m)**2 for x in ls_hz)/(len(ls_hz)-1)
        hz_std = round(var**0.5, 3)

    upd_mean = round(sum(ls_upd)/len(ls_upd), 3) if ls_upd else None
    upd_std = None
    if ls_upd and len(ls_upd) > 1:
        m = sum(ls_upd)/len(ls_upd)
        var = sum((x-m)**2 for x in ls_upd)/(len(ls_upd)-1)
        upd_std = round(var**0.5, 3)

    # Reward slope
    ts_col_rw = find_col(["timestamp_ms", "ts_ms", "ts", "time_ms", "time"], reward_cols)
    reward_col = find_col(["reward", "reward_value", "r"], reward_cols)
    rw_ts = [to_float_safe(r.get(ts_col_rw)) for r in reward_rows] if ts_col_rw else []
    rw_ts = [v for v in rw_ts if v is not None]
    rw_vals = [to_float_safe(r.get(reward_col)) for r in reward_rows] if reward_col else []
    rw_vals = [v for v in rw_vals if v is not None]
    rw_slope = slope_linear(rw_ts, rw_vals, np_mod) if rw_ts and rw_vals else None

    # Estimated intervals
    def median_delta(values: List[float]) -> Optional[float]:
        if len(values) < 2:
            return None
        deltas = sorted(values[i]-values[i-1] for i in range(1, len(values)))
        n = len(deltas)
        m = n//2
        return deltas[m] if n % 2 == 1 else (deltas[m-1]+deltas[m])/2.0

    # Prefer explicit run meta when available; integrity can be noisy for short runs
    memdb_interval_ms = None
    reward_interval_ms = None
    steps = None

    meta_path = None
    for name in ("run_meta.json", "production_meta.json"):
        candidate = os.path.join(exports_dir, name)
        if os.path.exists(candidate):
            meta_path = candidate
            break
    # Robust JSON loader to handle UTF-8 BOM and common Windows encodings
    def _load_json_any_encoding(path: str):
        encodings = ('utf-8', 'utf-8-sig', 'utf-16', 'utf-16-le', 'utf-16-be')
        for enc in encodings:
            try:
                with open(path, 'r', encoding=enc) as f:
                    return json.load(f)
            except Exception:
                pass
        try:
            with open(path, 'rb') as fb:
                data = fb.read()
            try:
                return json.loads(data.decode('utf-8-sig'))
            except Exception:
                return json.loads(data.decode('utf-8', errors='ignore'))
        except Exception:
            return {}

    meta = {}
    if meta_path:
        meta = _load_json_any_encoding(meta_path)
        # Aliases — always take explicit meta if present
        def read_meta_float(keys: List[str]) -> Optional[float]:
            for k in keys:
                v = meta.get(k)
                if v is not None:
                    try:
                        return float(v)
                    except Exception:
                        continue
            return None
        def read_meta_int(keys: List[str]) -> Optional[int]:
            for k in keys:
                v = meta.get(k)
                if v is not None:
                    try:
                        return int(v)
                    except Exception:
                        continue
            return None

        memdb_interval_ms = read_meta_float(["memdb_interval_ms", "telemetry_interval_ms", "mem_interval_ms", "memdb_interval"]) or memdb_interval_ms
        reward_interval_ms = read_meta_float(["reward_interval_ms", "reward_interval", "reward_tick_ms"]) or reward_interval_ms
        steps = read_meta_int(["steps", "total_steps", "run_steps"]) or steps

    # Fall back to integrity values only if meta was missing
    if memdb_interval_ms is None and isinstance(integrity.get("memdb_interval_ms"), (int, float)):
        memdb_interval_ms = float(integrity.get("memdb_interval_ms"))
    if reward_interval_ms is None and isinstance(integrity.get("reward_interval_ms"), (int, float)):
        reward_interval_ms = float(integrity.get("reward_interval_ms"))
    if steps is None and isinstance(integrity.get("steps"), (int, float)):
        steps = int(integrity.get("steps"))
    if memdb_interval_ms is None and ls_ts:
        memdb_interval_ms = median_delta(ls_ts)
    if reward_interval_ms is None and rw_ts:
        reward_interval_ms = median_delta(rw_ts)

    if steps is None and learning_rows:
        step_col = find_col(["step", "steps"], learning_cols)
        if step_col:
            try:
                steps = int(max(int(r.get(step_col)) for r in learning_rows if r.get(step_col) is not None))
            except Exception:
                steps = None

    # Plots
    reward_img = encode_plot_png(plt_mod, rw_ts, rw_vals, "Reward over time", "timestamp", "reward") if rw_ts and rw_vals else None
    hz_img = encode_plot_png(plt_mod, ls_ts, ls_hz, "Processing Hz over time", "timestamp", "Hz") if ls_ts and ls_hz else None

    # Markdown render
    now = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")
    lines: List[str] = []
    lines.append(f"# NeuroForge Production Run Report — {datetime.utcnow().strftime('%Y-%m-%d')}")
    lines.append(f"- Exports dir: {exports_dir}")
    lines.append(f"- Generated at: {now}")
    lines.append(f"- Telemetry interval: {memdb_interval_ms if memdb_interval_ms is not None else 'unknown'} ms")
    lines.append(f"- Reward interval: {reward_interval_ms if reward_interval_ms is not None else 'unknown'} ms")
    lines.append(f"- Steps: {steps if steps is not None else 'unknown'}")

    # Context configuration summary (Phase 17a/17b)
    if meta_path:
        meta = _load_json_any_encoding(meta_path)
        if isinstance(meta, dict):
            ctx_meta = meta.get("context", {}) if isinstance(meta.get("context", {}), dict) else {}
            peers = ctx_meta.get("peers", []) if isinstance(ctx_meta.get("peers", []), list) else []
            gain = ctx_meta.get("gain")
            kappa = ctx_meta.get("kappa")
            c_enabled = ctx_meta.get("couplings_enabled")
            couplings = ctx_meta.get("couplings", []) if isinstance(ctx_meta.get("couplings", []), list) else []
            if peers or gain is not None:
                lines.append("")
                lines.append("## Context Configuration")
                lines.append(f"- Context peers: {', '.join(peers) if peers else 'none'}")
                lines.append(f"- Context gain: {gain if gain is not None else 'n/a'}")
                if kappa is not None:
                    lines.append(f"- Context kappa: {kappa}")
                lines.append(f"- Couplings enabled: {('yes' if c_enabled else 'no') if c_enabled is not None else 'unknown'}")
                if couplings:
                    lines.append(f"- Couplings count: {len(couplings)}")
                    # Show a compact preview of up to 6 couplings
                    preview = []
                    for i, c in enumerate(couplings[:6]):
                        try:
                            preview.append(f"{c.get('src','?')}→{c.get('dst','?')}:{c.get('lambda','?')}")
                        except Exception:
                            pass
                    if preview:
                        lines.append(f"- Couplings preview: {', '.join(preview)}")

    if integrity:
        passed = integrity.get("passed")
        exp_rows = integrity.get("expected_learning_rows")
        actual_rows = len(learning_rows)
        status = "✅ passed" if passed else "⚠️ failed"
        lines.append(f"- Integrity: {status} (expected {exp_rows}, found {actual_rows})")

    lines.append("\n## Learning Stats")
    lines.append("| Metric | Mean | Std |")
    lines.append("|---------|------|-----|")
    lines.append(f"| processing_hz | {hz_mean if hz_mean is not None else 'n/a'} | {hz_std if hz_std is not None else 'n/a'} |")
    lines.append(f"| updates | {upd_mean if upd_mean is not None else 'n/a'} | {upd_std if upd_std is not None else 'n/a'} |")

    lines.append("\n## Reward Dynamics")
    lines.append(f"- Reward slope: {round(rw_slope, 5) if rw_slope is not None else 'n/a'}")
    if reward_img is not None:
        lines.append("")
        lines.append(reward_img)
    else:
        lines.append("(Plot unavailable — matplotlib not installed or missing data)")

    lines.append("\n## Processing Hz Trend")
    if hz_img is not None:
        lines.append("")
        lines.append(hz_img)
    else:
        lines.append("(Plot unavailable — matplotlib not installed or missing data)")

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(lines))
    print(f"Production report written: {out_path}")

if __name__ == "__main__":
    main()
