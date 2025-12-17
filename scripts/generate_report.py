#!/usr/bin/env python3
"""
Generate a Markdown report (Artifacts/REPORT.md) summarizing benchmark and analysis outputs.

Sections:
 - Overview and run metadata
 - Tables: analysis_summary.csv and SUMMARY/all_results.csv (if present)
 - Figures: embed key plots from Artifacts/PNG/benchmarks and Artifacts/PNG/analysis
"""
import os
import csv
import glob
from pathlib import Path
import subprocess
from datetime import datetime


def read_csv_rows(path: Path):
    rows = []
    if not path.exists():
        return rows
    with open(path, 'r', encoding='utf-8') as f:
        r = csv.reader(f)
        headers = next(r, None)
        for row in r:
            rows.append(row)
    return rows


def embed_images_md(paths):
    lines = []
    for p in paths:
        rel = p.replace('\\', '/')
        lines.append(f"![]({rel})")
    return '\n\n'.join(lines)


def main():
    repo_root = Path(__file__).resolve().parent.parent
    artifacts = repo_root / 'Artifacts'
    out_md = artifacts / 'REPORT.md'
    artifacts.mkdir(parents=True, exist_ok=True)

    # Summary CSVs
    analysis_summary = artifacts / 'CSV' / 'analysis' / 'analysis_summary.csv'
    collated_summary = artifacts / 'SUMMARY' / 'all_results.csv'
    derived_metrics = artifacts / 'CSV' / 'derived' / 'time_series_metrics.csv'
    stats_summary = artifacts / 'CSV' / 'stats' / 'stat_tests_summary.csv'
    analysis_rows = read_csv_rows(analysis_summary)
    collated_rows = read_csv_rows(collated_summary)
    derived_rows = read_csv_rows(derived_metrics)
    stats_rows = read_csv_rows(stats_summary)

    # Figures
    bench_figs = sorted(glob.glob(str(artifacts / 'PNG' / 'benchmarks' / '**' / '*.png'), recursive=True))
    analysis_figs = sorted(glob.glob(str(artifacts / 'PNG' / 'analysis' / '**' / '*.png'), recursive=True))

    # Build report
    lines = []
    lines.append(f"# NeuroForge Benchmark & Analysis Report\n")
    # Git commit and build info
    commit = None
    try:
        commit = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], cwd=str(repo_root)).decode('utf-8').strip()
    except Exception:
        commit = None
    exe_path = repo_root / 'build' / 'neuroforge.exe'
    build_info = None
    if exe_path.exists():
        try:
            mtime = datetime.utcfromtimestamp(exe_path.stat().st_mtime).isoformat() + 'Z'
            size_mb = exe_path.stat().st_size / (1024*1024)
            build_info = f"{mtime}, {size_mb:.2f} MB"
        except Exception:
            build_info = None

    lines.append(f"Generated: {datetime.utcnow().isoformat()}Z\n")
    if commit:
        lines.append(f"Commit: `{commit}`")
    if build_info:
        lines.append(f"Build: `{exe_path}` ({build_info})")

    lines.append("## Overview")
    lines.append("- Artifacts root: `Artifacts/`")
    lines.append("- Includes unified substrate benchmarks, Transformer embeddings, RSA/CKA analysis, and CSV summaries.")

    if analysis_rows:
        lines.append("\n## Analyzer Summary (analysis_summary.csv)")
        lines.append("| file | rows | steps_min | steps_max | coh_mean | coh_var | asm_final | growth_total |")
        lines.append("|---|---:|---:|---:|---:|---:|---:|---:|")
        for row in analysis_rows:
            # file, rows, steps_min, steps_max, coh_mean, coh_var, asm_final, growth_total
            cells = [row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7]]
            lines.append('| ' + ' | '.join(cells) + ' |')

    if collated_rows:
        lines.append("\n## Collated Results (SUMMARY/all_results.csv)")
        lines.append("| experiment | file | layer | metric | value |")
        lines.append("|---|---|---:|---|---:|")
        for row in collated_rows:
            # experiment, file, layer, metric, value
            cells = [row[0], row[1], row[2], row[3], row[4]]
            lines.append('| ' + ' | '.join(cells) + ' |')

    if derived_rows:
        lines.append("\n## Derived Time-Series Metrics (time_series_metrics.csv)")
        lines.append("| series_name | experiment | time_to_first_assembly | median_coherence_last | damping_ratio | growth_total |")
        lines.append("|---|---|---:|---:|---:|---:|")
        # Find indices by header
        # file, series_name, experiment, steps_total, steps_min, steps_max, time_to_first_assembly, median_coherence_last_X, damping_ratio_X, final_assemblies, growth_total
        for row in derived_rows:
            series_name = row[1]
            experiment = row[2]
            tffa = row[6]
            # Search for median and damping columns
            med = next((c for c in row[7:10] if c != ''), '')
            damp = next((c for c in row[7:10] if c != '' and c != med), '')
            growth = row[10] if len(row) > 10 else ''
            cells = [series_name, experiment, tffa, med, damp, growth]
            lines.append('| ' + ' | '.join(cells) + ' |')

    if stats_rows:
        lines.append("\n## Statistical Tests (stat_tests_summary.csv)")
        lines.append("| metric | layer | mean_diff | 95% CI low | 95% CI high | p (t) | p (Wilcoxon) | n_pairs |")
        lines.append("|---|---|---:|---:|---:|---:|---:|---:|")
        for row in stats_rows:
            # metric, layer, mean_diff, t_p, w_p, boot_low, boot_high, n_pairs
            cells = [row[0], row[1], row[2], row[5], row[6], row[3], row[4], row[7]]
            lines.append('| ' + ' | '.join(cells) + ' |')

    if bench_figs:
        lines.append("\n## Benchmark Figures")
        # Embed up to 12 benchmark images
        lines.append(embed_images_md([os.path.relpath(p, repo_root) for p in bench_figs[:12]]))

    if analysis_figs:
        lines.append("\n## Analysis Figures (RSA/CKA)")
        # Embed up to 12 analysis images
        lines.append(embed_images_md([os.path.relpath(p, repo_root) for p in analysis_figs[:12]]))

    # Stats plots
    stats_fig = artifacts / 'PNG' / 'stats' / 'stat_tests_boxplots.png'
    if stats_fig.exists():
        lines.append("\n## Statistical Plots")
        lines.append(embed_images_md([os.path.relpath(str(stats_fig), repo_root)]))

    # Narrative summary (template)
    try:
        lines.append("\n## System & Cognitive Performance (Template)")
        lines.append("This section summarizes throughput, functional behavior, representational alignment, decodability, and dynamics with statistical validation.")
        # Pull key stats if available
        key_rows = {}
        metric_map = {
            'time_to_first_assembly': 'Time-to-first-assembly',
            'median_coherence_last_300': 'Median coherence (last 300)',
            'damping_ratio_300': 'Damping ratio (late/early)'
        }
        if stats_rows:
            for row in stats_rows:
                m = row[0]
                if m in metric_map and m not in key_rows:
                    key_rows[m] = row
        # Compose sentences
        paragraph = []
        if key_rows.get('time_to_first_assembly'):
            r = key_rows['time_to_first_assembly']
            paragraph.append(f"Adaptive vs Fixed reduces {metric_map['time_to_first_assembly']} by {float(r[2]):.1f} (95% CI [{r[5]}, {r[6]}], t-p={r[3]}, w-p={r[4]}).")
        if key_rows.get('median_coherence_last_300'):
            r = key_rows['median_coherence_last_300']
            paragraph.append(f"Adaptive increases {metric_map['median_coherence_last_300']} by {float(r[2]):.3f} (95% CI [{r[5]}, {r[6]}], t-p={r[3]}, w-p={r[4]}).")
        if key_rows.get('damping_ratio_300'):
            r = key_rows['damping_ratio_300']
            paragraph.append(f"Adaptive lowers {metric_map['damping_ratio_300']} by {float(r[2]):.2f} (95% CI [{r[5]}, {r[6]}], t-p={r[3]}, w-p={r[4]}), indicating stabilization.")
        if paragraph:
            lines.append(' '.join(paragraph))
        else:
            lines.append("Template: Add sentences here summarizing A/B differences (time-to-assembly, coherence, damping) and RSA/CKA alignment.")
    except Exception:
        pass

    out_md.write_text('\n'.join(lines), encoding='utf-8')
    print(f"Wrote report → {out_md}")


if __name__ == '__main__':
    main()
