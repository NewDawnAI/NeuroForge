#!/usr/bin/env python3
"""
Generate small-multiples figures from Phase C sweep_summary.csv.

- Reads: build/Release/Sweeps/sweep_summary.csv (default)
- Produces: build/Release/Sweeps/SmallMultiples_SequenceAccuracy.svg (+ optional PNG)

Layout:
- One heatmap per capacity value
- Heatmap axes: x = seq_window, y = decay
- Cell value: selectable metric column (default: sequence_accuracy)
- Consistent color scale across panels
- Optional best-cell outline, optional investor mode to hide (n=...) annotations

Standard library + matplotlib only.
"""
import argparse
import csv
import os
from collections import defaultdict
from typing import Dict, List, Tuple
import json

# Matplotlib headless backend
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import Normalize
from matplotlib.cm import ScalarMappable
from matplotlib.patches import Rectangle
from matplotlib.colors import to_rgb, LinearSegmentedColormap


def read_summary_csv(path: str) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    with open(path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append(r)
    if not rows:
        raise RuntimeError(f"No rows found in summary CSV: {path}")
    return rows


def parse_unique(values: List[str], cast):
    uniq = sorted({cast(v) for v in values})
    return uniq


def prepare_grids(rows: List[Dict[str, str]], metric_col: str):
    capacities = parse_unique([r['capacity'] for r in rows], int)
    decays = parse_unique([r['decay'] for r in rows], float)
    windows = parse_unique([r['seq_window'] for r in rows], int)

    # Build maps per capacity -> (decay, window) -> (metric_val, n)
    by_cap: Dict[int, Dict[Tuple[float, int], Tuple[float, int]]] = defaultdict(dict)
    for r in rows:
        try:
            cap = int(r['capacity'])
            dec = float(r['decay'])
            win = int(r['seq_window'])
            val_str = r.get(metric_col)
            n = r.get('sequence_n')
            val = float(val_str) if (val_str is not None and val_str != '' and str(val_str).lower() != 'none') else np.nan
            n_i = int(n) if (n is not None and n != '') else 0
        except Exception:
            # Skip malformed row
            continue
        by_cap[cap][(dec, win)] = (val, n_i)

    return capacities, decays, windows, by_cap


def build_matrix(decays: List[float], windows: List[int], grid: Dict[Tuple[float, int], Tuple[float, int]]):
    H = len(decays)
    W = len(windows)
    val_mat = np.full((H, W), np.nan, dtype=float)
    n_mat = np.zeros((H, W), dtype=int)
    for i, d in enumerate(decays):
        for j, w in enumerate(windows):
            val, n = grid.get((d, w), (np.nan, 0))
            val_mat[i, j] = val
            n_mat[i, j] = n
    return val_mat, n_mat


def compute_global_range(capacities, decays, windows, by_cap):
    mins = []
    maxs = []
    for cap in capacities:
        val_mat, _ = build_matrix(decays, windows, by_cap.get(cap, {}))
        if not np.all(np.isnan(val_mat)):
            mins.append(np.nanmin(val_mat))
            maxs.append(np.nanmax(val_mat))
    if not mins or not maxs:
        return 0.0, 1.0
    vmin = float(min(mins))
    vmax = float(max(maxs))
    if not np.isfinite(vmin) or not np.isfinite(vmax) or vmin == vmax:
        return 0.0, 1.0
    return vmin, vmax


def make_brand_cmap(accent_hex: str) -> LinearSegmentedColormap:
    """Construct a light -> accent -> dark ramp based on a brand accent color."""
    r, g, b = to_rgb(accent_hex)
    # Lighten toward white (85%) and darken toward black (25%)
    light = (1 - 0.85 * (1 - r), 1 - 0.85 * (1 - g), 1 - 0.85 * (1 - b))
    dark = (0.25 * r, 0.25 * g, 0.25 * b)
    return LinearSegmentedColormap.from_list('brand_ramp', [light, (r, g, b), dark], N=256)


def plot_small_multiples(capacities, decays, windows, by_cap, out_svg: str,
                         out_png: str = None,
                         highlight_best: bool = True,
                         outline_color: str = 'red',
                         investor_mode: bool = False,
                         font_scale: float = 1.0,
                         metric: str = 'sequence_accuracy',
                         cmap=None,
                         json_only: bool = False):
    # One panel per capacity
    n_caps = len(capacities)
    fig_w = max(5.0, 4.0 * n_caps)
    fig_h = 6.0
    fig, axes = plt.subplots(1, n_caps, figsize=(fig_w, fig_h), squeeze=False)
    axes = axes[0]

    # Decide normalization
    if metric == 'sequence_accuracy':
        vmin, vmax = 0.0, 1.0
    else:
        vmin, vmax = compute_global_range(capacities, decays, windows, by_cap)
    norm = Normalize(vmin=vmin, vmax=vmax)
    if cmap is None:
        cmap = plt.get_cmap('viridis')

    # Font sizing
    fs_title = 14 * font_scale
    fs_label = 12 * font_scale
    fs_tick = 10 * font_scale
    fs_anno = 9 * font_scale

    # Data range check to set title note
    has_any_val = False

    # Midpoint for determining annotation text color
    mid = vmin + 0.5 * (vmax - vmin)

    for idx, cap in enumerate(capacities):
        ax = axes[idx]
        val_mat, n_mat = build_matrix(decays, windows, by_cap.get(cap, {}))
        if not np.all(np.isnan(val_mat)):
            has_any_val = True
        # Display heatmap
        im = ax.imshow(val_mat, aspect='auto', origin='upper', cmap=cmap, norm=norm)

        # Annotate cells
        for i, d in enumerate(decays):
            for j, w in enumerate(windows):
                val = val_mat[i, j]
                n = n_mat[i, j]
                if np.isnan(val):
                    txt = '—'
                    color = 'lightgray'
                else:
                    if metric == 'sequence_accuracy':
                        txt = f"{val*100:.0f}%"
                        color = 'white' if val >= 0.5 else 'black'
                    else:
                        txt = f"{val:.2g}"
                        color = 'white' if val >= mid else 'black'
                # include n unless investor mode
                if (not investor_mode) and (n > 0) and (not np.isnan(val)):
                    txt += f"\n(n={n})"
                ax.text(j, i, txt, ha='center', va='center', fontsize=fs_anno, color=color)

        # Optional: highlight best cell per panel
        if highlight_best and not np.all(np.isnan(val_mat)):
            try:
                max_val = np.nanmax(val_mat)
                locations = np.argwhere(val_mat == max_val)
                if locations.size > 0:
                    i_best, j_best = locations[0]
                    rect = Rectangle((j_best - 0.5, i_best - 0.5), 1, 1,
                                     fill=False, edgecolor=outline_color, linewidth=2.5)
                    ax.add_patch(rect)
            except Exception:
                pass

        # Axis ticks/labels
        ax.set_title(f"Capacity = {cap}", fontsize=fs_title)
        ax.set_xticks(range(len(windows)))
        ax.set_xticklabels([str(w) for w in windows], fontsize=fs_tick)
        ax.set_xlabel('seq_window', fontsize=fs_label)
        ax.set_yticks(range(len(decays)))
        ax.set_yticklabels([str(d) for d in decays], fontsize=fs_tick)
        ax.set_ylabel('decay', fontsize=fs_label)
        ax.grid(False)

    # Shared colorbar
    cbar = fig.colorbar(ScalarMappable(norm=norm, cmap=cmap), ax=axes.tolist(), shrink=0.9, pad=0.02)
    cbar.set_label(metric.replace('_', ' ').title(), fontsize=fs_label)
    cbar.ax.tick_params(labelsize=fs_tick)

    fig.suptitle(f'Phase C — Small Multiples ({metric.replace("_"," ").title()} by seq_window × decay per Capacity)', fontsize=fs_title)
    fig.tight_layout(rect=[0, 0.05, 1, 0.95])

    # Footer notes
    if metric != 'sequence_accuracy':
        fig.text(0.5, 0.015, 'Global color scale across panels (auto-scaled for metric)',
                 ha='center', va='bottom', fontsize=8 * font_scale, color='dimgray')
    if not has_any_val:
        fig.text(0.5, 0.005, f'Note: {metric} missing (NaN) — ensure the metric is logged during sweeps',
                 ha='center', va='bottom', fontsize=8 * font_scale, color='red')

    os.makedirs(os.path.dirname(out_svg), exist_ok=True)
    fig.savefig(out_svg, format='svg')
    if not json_only:
        print(f"[small-multiples] metric={metric} investor_mode={investor_mode} outline_color={outline_color} font_scale={font_scale} cmap={'brand' if isinstance(cmap, LinearSegmentedColormap) and cmap.name=='brand_ramp' else 'viridis'}")
        print(f"[small-multiples] Wrote SVG: {out_svg}")
    if out_png:
        try:
            fig.savefig(out_png, format='png', dpi=300)
            if not json_only:
                print(f"[small-multiples] Wrote PNG: {out_png}")
        except Exception as e:
            if not json_only:
                print(f"[small-multiples] WARN: failed to write PNG ({e})")


def main():
    parser = argparse.ArgumentParser(description='Generate small-multiples figure from sweep_summary.csv')
    parser.add_argument('--summary', type=str, default=os.path.join('build', 'Release', 'Sweeps', 'sweep_summary.csv'),
                        help='Path to sweep_summary.csv')
    parser.add_argument('--out', type=str, default=os.path.join('build', 'Release', 'Sweeps', 'SmallMultiples_SequenceAccuracy.svg'),
                        help='Output SVG path')
    parser.add_argument('--png-out', type=str, default=os.path.join('build', 'Release', 'Sweeps', 'SmallMultiples_SequenceAccuracy.png'),
                        help='Optional PNG output path (set empty string to skip)')
    parser.add_argument('--no-highlight', dest='highlight', action='store_false', help='Disable best-cell highlight outline')
    parser.add_argument('--outline-color', type=str, default='red', help='Highlight outline color (e.g., red, #2E86DE)')
    parser.add_argument('--investor-mode', action='store_true', help='Hide (n=...) annotations for investor-facing deck')
    parser.add_argument('--font-scale', type=float, default=1.0, help='Scale factor for titles/labels/annotations')
    parser.add_argument('--metric', type=str, default='sequence_accuracy', help='Metric column to visualize')
    parser.add_argument('-P', '--deck-preset', action='store_true', help='Preset for deck export: investor-mode, font-scale=1.15, outline-color=#2E86DE')
    parser.add_argument('-Pa', action='store_true', help='Deck preset + accuracy metric')
    parser.add_argument('-Ps', action='store_true', help='Deck preset + strength metric')
    parser.add_argument('-Pm', action='store_true', help='Deck preset + max entries metric')
    parser.add_argument('-Pv', action='store_true', help='Deck preset + avg entries metric')
    parser.add_argument('--brand-color', type=str, default=None, help='Brand accent hex color for deck/export (aliases outline color + brand colormap accent)')
    parser.add_argument('--phase-c-log-json', action='store_true', help='Emit line-delimited JSON to stdout for dashboards/live runs')
    parser.add_argument('--json-only', action='store_true', help='Suppress human-readable logs; only emit JSON when --phase-c-log-json is enabled')
    parser.set_defaults(highlight=True)
    args = parser.parse_args()

    rows = read_summary_csv(args.summary)

    # Apply metric aliases
    alias_map = {
        'acc': 'sequence_accuracy',
        'avg': 'avg_token_entries',
        'max': 'max_token_entries',
        'strength': 'strength_role_token',
    }
    if args.metric in alias_map:
        args.metric = alias_map[args.metric]

    # Apply shorthand presets for metric + deck
    shorthand_used = False
    if args.Pa:
        args.metric = 'sequence_accuracy'
        shorthand_used = True
    if args.Ps:
        args.metric = 'strength_role_token'
        shorthand_used = True
    if args.Pm:
        args.metric = 'max_token_entries'
        shorthand_used = True
    if args.Pv:
        args.metric = 'avg_token_entries'
        shorthand_used = True
    if shorthand_used:
        args.deck_preset = True

    # Validate metric column existence, fallback to sequence_accuracy if missing
    if rows and (args.metric not in rows[0]):
        if not args.json_only:
            print(f"[small-multiples] WARN: metric '{args.metric}' not found. Falling back to 'sequence_accuracy'.")
        args.metric = 'sequence_accuracy'

    # Apply deck preset (takes precedence)
    if args.deck_preset:
        args.investor_mode = True
        args.font_scale = 1.15
        args.outline_color = '#2E86DE'
        brand_cmap = make_brand_cmap(args.outline_color)
    else:
        brand_cmap = None

    # Apply brand-color override (more discoverable): sets outline + brand ramp
    if args.brand_color:
        args.outline_color = args.brand_color
        brand_cmap = make_brand_cmap(args.brand_color)

    capacities, decays, windows, by_cap = prepare_grids(rows, args.metric)

    if not capacities:
        raise RuntimeError('No capacities found in summary CSV')
    if not decays or not windows:
        raise RuntimeError('Missing decays or seq_window values in summary CSV')

    # JSON logging helper
    def _logj(obj):
        if args.phase_c_log_json:
            try:
                print(json.dumps(obj, ensure_ascii=False))
            except Exception:
                pass

    # Compute normalization range (to log it)
    if args.metric == 'sequence_accuracy':
        _vmin, _vmax = 0.0, 1.0
    else:
        _vmin, _vmax = compute_global_range(capacities, decays, windows, by_cap)

    # Emit config + scale
    _logj({
        'event': 'config',
        'summary_path': args.summary,
        'out_svg': args.out,
        'out_png': args.png_out or '',
        'metric': args.metric,
        'investor_mode': args.investor_mode,
        'font_scale': args.font_scale,
        'outline_color': args.outline_color,
        'deck_preset': bool(args.deck_preset),
        'cmap': 'brand' if (brand_cmap is not None) else 'viridis',
        'json_only': bool(args.json_only)
    })
    _logj({'event': 'scale', 'vmin': _vmin, 'vmax': _vmax})

    # Emit best cell per panel
    for cap in capacities:
        val_mat, n_mat = build_matrix(decays, windows, by_cap.get(cap, {}))
        if np.all(np.isnan(val_mat)):
            continue
        try:
            max_val = float(np.nanmax(val_mat))
            locs = np.argwhere(val_mat == max_val)
            i_best, j_best = (int(locs[0][0]), int(locs[0][1])) if locs.size > 0 else (None, None)
            record = {
                'event': 'panel_best',
                'capacity': int(cap),
                'value': max_val,
                'decay': float(decays[i_best]) if i_best is not None else None,
                'seq_window': int(windows[j_best]) if j_best is not None else None,
                'n': int(n_mat[i_best, j_best]) if i_best is not None else None
            }
            _logj(record)
        except Exception:
            pass

    png_out = args.png_out if args.png_out else None
    plot_small_multiples(capacities, decays, windows, by_cap,
                         args.out, png_out,
                         args.highlight, args.outline_color,
                         args.investor_mode, args.font_scale, args.metric,
                         cmap=brand_cmap,
                         json_only=args.json_only)

    # Emit output completion markers
    _logj({'event': 'output', 'type': 'svg', 'path': args.out})
    if png_out:
        _logj({'event': 'output', 'type': 'png', 'path': png_out})
    _logj({'event': 'done'})


if __name__ == '__main__':
    main()