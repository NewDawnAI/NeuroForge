#!/usr/bin/env python3
import argparse
import csv
import os
import glob
import statistics
import json


def read_spike_metrics(spike_path):
    with open(spike_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        spike_rows = len(rows)
        unique_neurons = len({r.get('neuron_id') for r in rows if 'neuron_id' in r})
        return spike_rows, unique_neurons


def read_synapse_metrics(syn_path):
    with open(syn_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        weights = []
        for r in reader:
            w = r.get('weight')
            if w is not None:
                try:
                    weights.append(float(w))
                except ValueError:
                    continue
        syn_rows = len(weights)
        if syn_rows:
            w_min = min(weights)
            w_max = max(weights)
            w_mean = statistics.mean(weights)
            w_std = statistics.stdev(weights) if syn_rows >= 2 else 0.0
            ws = sorted(weights)
            idx95 = max(0, int(0.95 * syn_rows) - 1)
            idx99 = max(0, int(0.99 * syn_rows) - 1)
            w_p95 = ws[idx95] if syn_rows else 0.0
            w_p99 = ws[idx99] if syn_rows else 0.0
        else:
            w_min = w_max = w_mean = 0.0
            w_std = w_p95 = w_p99 = 0.0
        return syn_rows, w_min, w_max, w_mean, w_std, w_p95, w_p99


def pair_files(dir_path, prefix):
    syn_files = sorted(glob.glob(os.path.join(dir_path, f'{prefix}_synapses*.csv')))
    spike_files = sorted(glob.glob(os.path.join(dir_path, f'{prefix}_spikes*.csv')))
    spike_map = {os.path.basename(p).replace(f'{prefix}_spikes', ''): p for p in spike_files}
    pairs = []
    for syn in syn_files:
        suffix = os.path.basename(syn).replace(f'{prefix}_synapses', '')
        spike = spike_map.get(suffix)
        pairs.append((syn, spike))
    return pairs


def write_markdown_table(md_path, rows, columns=None):
    header_map = {
        'run_label': 'Run',
        'synapse_rows': 'Synapse rows',
        'weight_min': 'Weight min',
        'weight_max': 'Weight max',
        'weight_mean': 'Weight mean',
        'weight_stddev': 'Weight stddev',
        'weight_p95': 'Weight p95',
        'weight_p99': 'Weight p99',
        'spike_rows': 'Spike rows',
        'unique_spiking_neurons': 'Unique neurons',
    }
    if columns is None:
        columns = ['run_label', 'synapse_rows', 'weight_mean', 'spike_rows', 'unique_spiking_neurons']
    headers = [header_map.get(c, c) for c in columns]
    with open(md_path, 'w', encoding='utf-8') as f:
        f.write('| ' + ' | '.join(headers) + ' |\n')
        # Align first column left, others right
        align = '|---|' + '---:|' * (len(headers) - 1) + '\n'
        f.write(align)
        for r in rows:
            vals = [str(r.get(c, '')) for c in columns]
            f.write('| ' + ' | '.join(vals) + ' |\n')


def main():
    epilog = r"""

Usage Recipes:

1.  **Default Autonomy Summary (CSV + Markdown)**
    Generates `autonomy_summary.csv` and `autonomy_summary.md` in `Artifacts/CSV`,
    sorted by synapse count descending, with 3-decimal-place float precision.
    Discovers `Artifacts/CSV/label_map.json` automatically.

    ```sh
    python tools/summarize_csv.py --dir Artifacts/CSV --prefix autonomy \
      --out autonomy_summary.csv --md-out autonomy_summary.md \
      --sort-by synapse_rows --desc --float-format .3f
    ```

2.  **Live Summary with JSON Output and Filtering**
    Processes `live_*` CSVs, filters for specific runs, and outputs all three formats
    (CSV, Markdown, JSON).

    ```sh
    python tools/summarize_csv.py --dir Artifacts/CSV --prefix live \
      --out live_summary.csv --md-out live_summary.md --json-out live_summary.json \
      --filter-label "Baseline,Sweep B" --sort-by weight_mean --float-format .4f
    ```

3.  **Custom Columns for Markdown Table**
    Generates a Markdown table with only the specified columns.

    ```sh
    python tools/summarize_csv.py --dir Artifacts/CSV --prefix autonomy \
      --md-out custom_summary.md --columns "run_label,synapse_rows,weight_p99,spike_rows"
    ```

4.  **Tagging and Hypothesis Grouping**
    Injects tags from a JSON map and filters by tags.

    ```sh
    # tag_map.json example: {"Baseline": "control", "Sweep B": ["hypothesis-42", "lr=1e-3"]}
    python tools/summarize_csv.py --dir Artifacts/CSV --prefix autonomy \
      --out autonomy_summary.csv --md-out autonomy_summary.md --json-out autonomy_summary.json \
      --tag-map Artifacts/CSV/tag_map.json --filter-tag "control,hypothesis-42" \
      --sort-by synapse_rows --desc --float-format .3f
    ```

Available Columns for Sorting/Display:
- run_label
- synapses_file, spikes_file
- synapse_rows, spike_rows, unique_spiking_neurons
- weight_min, weight_max, weight_mean, weight_stddev, weight_p95, weight_p99
- tags

"""
    ap = argparse.ArgumentParser(
        description='Summarize CSV pairs with a given prefix (synapses + spikes).',
        epilog=epilog,
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument('--dir', default='.', help='Directory containing CSVs (default: .)')
    ap.add_argument('--prefix', default='autonomy', help='File prefix (default: autonomy)')
    ap.add_argument('--out', default='summary.csv', help='Output summary CSV filename')
    ap.add_argument('--md-out', default=None, help='Optional output Markdown table filename')
    ap.add_argument('--columns', default=None, help='Comma-separated summary keys for Markdown table (optional)')
    ap.add_argument('--suffix-map', default=None, help='Comma-separated suffix=label pairs to alias run labels (e.g., sweep_A=Sweep A,baseline=Baseline)')
    ap.add_argument('--label-map', default=None, help='Path to JSON file mapping raw suffix to label (e.g., {"sweep_A": "Sweep A"})')
    ap.add_argument('--sort-by', default=None, help='Column to sort rows by for CSV/Markdown output (e.g., synapse_rows, weight_mean)')
    ap.add_argument('--desc', action='store_true', help='Sort descending when used with --sort-by')
    ap.add_argument('--float-format', default='.6g', help='Python format spec for float values (e.g., .3f, .6g)')
    ap.add_argument('--json-out', default=None, help='Optional output JSON filename')
    ap.add_argument('--filter-label', default=None, help='Comma-separated run labels to include')
    ap.add_argument('--exclude-label', default=None, help='Comma-separated run labels to exclude')
    ap.add_argument('--fail-on-missing', action='store_true', help='Exit with error if a spikes file is missing')
    ap.add_argument('--tag-map', default=None, help='Path to JSON mapping of final run labels to tags (string or list)')
    ap.add_argument('--filter-tag', default=None, help='Comma-separated tags to include')
    ap.add_argument('--exclude-tag', default=None, help='Comma-separated tags to exclude')
    args = ap.parse_args()

    dir_path = os.path.abspath(args.dir)
    pairs = pair_files(dir_path, args.prefix)
    if not pairs:
        print(f'No {args.prefix}_* CSVs found in {dir_path}')
        return 1

    # Fail early if requested and spikes are missing for any synapses
    if args.fail_on_missing:
        missing = [syn for syn, spike in pairs if not spike]
        if missing:
            print('Error: some synapse files have no matching spikes:')
            for m in missing:
                print(f' - {os.path.basename(m)}')
            return 2

    out_path = os.path.join(dir_path, args.out) if not os.path.isabs(args.out) else args.out
    fieldnames = [
        'run_label', 'synapses_file', 'spikes_file', 'synapse_rows',
        'weight_min', 'weight_max', 'weight_mean', 'weight_stddev', 'weight_p95', 'weight_p99',
        'spike_rows', 'unique_spiking_neurons', 'tags'
    ]

    # Parse suffix mapping once
    suffix_map = {}
    if args.suffix_map:
        for pair in args.suffix_map.split(','):
            if '=' in pair:
                k, v = pair.split('=', 1)
                k = k.strip()
                v = v.strip()
                if k:
                    suffix_map[k] = v if v else k

    # Load label map JSON if provided
    label_map = {}
    if args.label_map:
        lm_path = os.path.abspath(args.label_map)
        try:
            with open(lm_path, 'r', encoding='utf-8') as jf:
                data = json.load(jf)
                if isinstance(data, dict):
                    # normalize keys/values
                    for k, v in data.items():
                        if isinstance(k, str):
                            label_map[k.strip()] = str(v).strip()
        except Exception as e:
            print(f'Warning: failed to load label map from {lm_path}: {e}')

    # Auto-discover label_map.json in --dir if not provided
    if not label_map and not args.label_map:
        lm_default = os.path.join(dir_path, 'label_map.json')
        if os.path.exists(lm_default):
            try:
                with open(lm_default, 'r', encoding='utf-8') as jf:
                    data = json.load(jf)
                    if isinstance(data, dict):
                        for k, v in data.items():
                            label_map[str(k).strip()] = str(v).strip()
            except Exception as e:
                print(f'Warning: failed to load default label map from {lm_default}: {e}')

    # Load tag map JSON if provided
    tag_map = {}
    if args.tag_map:
        tm_path = os.path.abspath(args.tag_map)
        try:
            with open(tm_path, 'r', encoding='utf-8') as tf:
                data = json.load(tf)
                if isinstance(data, dict):
                    for k, v in data.items():
                        key = str(k).strip()
                        if isinstance(v, list):
                            tag_map[key] = [str(x).strip() for x in v if str(x).strip()]
                        elif isinstance(v, str):
                            v = v.strip()
                            tag_map[key] = [v] if v else []
                        else:
                            tag_map[key] = []
        except Exception as e:
            print(f'Warning: failed to load tag map from {tm_path}: {e}')

    summary_rows = []

    # Build rows first (so we can sort before writing)
    for syn_path, spike_path in pairs:
        run_label_raw = os.path.basename(syn_path).replace(f'{args.prefix}_synapses', '').replace('.csv', '').strip('_')
        # precedence: label_map > suffix_map > raw
        run_label_mapped = label_map.get(run_label_raw, suffix_map.get(run_label_raw, run_label_raw))
        syn_rows, w_min, w_max, w_mean, w_std, w_p95, w_p99 = read_synapse_metrics(syn_path)
        spike_rows, unique_neurons = read_spike_metrics(spike_path) if spike_path else (0, 0)
        # tags for final mapped label
        tags_list = tag_map.get(run_label_mapped, [])
        tags_str = ','.join(sorted(set(tags_list))) if tags_list else ''
        row = {
            'run_label': run_label_mapped or '(unknown)',
            'synapses_file': os.path.basename(syn_path),
            'spikes_file': os.path.basename(spike_path) if spike_path else '',
            'synapse_rows': syn_rows,
            'weight_min': format(w_min, args.float_format),
            'weight_max': format(w_max, args.float_format),
            'weight_mean': format(w_mean, args.float_format),
            'weight_stddev': format(w_std, args.float_format),
            'weight_p95': format(w_p95, args.float_format),
            'weight_p99': format(w_p99, args.float_format),
            'spike_rows': spike_rows,
            'unique_spiking_neurons': unique_neurons,
            'tags': tags_str,
        }
        summary_rows.append(row)

    # Apply label filters if any
    if args.filter_label:
        allowed = {s.strip() for s in args.filter_label.split(',') if s.strip()}
        summary_rows = [r for r in summary_rows if r.get('run_label') in allowed]
    if args.exclude_label:
        banned = {s.strip() for s in args.exclude_label.split(',') if s.strip()}
        summary_rows = [r for r in summary_rows if r.get('run_label') not in banned]

    # Apply tag filters if any
    def row_tags_set(r):
        t = r.get('tags') or ''
        return {x.strip() for x in t.split(',') if x.strip()}
    if args.filter_tag:
        allowed_tags = {s.strip() for s in args.filter_tag.split(',') if s.strip()}
        summary_rows = [r for r in summary_rows if row_tags_set(r) & allowed_tags]
    if args.exclude_tag:
        banned_tags = {s.strip() for s in args.exclude_tag.split(',') if s.strip()}
        summary_rows = [r for r in summary_rows if not (row_tags_set(r) & banned_tags)]

    # Apply sorting if requested
    if args.sort_by:
        sort_key = args.sort_by
        numeric_keys = {'synapse_rows', 'spike_rows', 'unique_spiking_neurons', 'weight_min', 'weight_max', 'weight_mean', 'weight_stddev', 'weight_p95', 'weight_p99'}
        int_keys = {'synapse_rows', 'spike_rows', 'unique_spiking_neurons'}
        def sort_val(r):
            v = r.get(sort_key)
            if sort_key in int_keys:
                try:
                    return int(v)
                except Exception:
                    return -1
            if sort_key in numeric_keys:
                try:
                    return float(v)
                except Exception:
                    return float('nan')
            return str(v) if v is not None else ''
        summary_rows.sort(key=sort_val, reverse=args.desc)

    # Write CSV
    with open(out_path, 'w', newline='') as fout:
        writer = csv.DictWriter(fout, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(summary_rows)

    print(f'Wrote summary: {out_path}')

    # Write Markdown (optional)
    if args.md_out:
        md_path = os.path.join(dir_path, args.md_out) if not os.path.isabs(args.md_out) else args.md_out
        columns = [c.strip() for c in args.columns.split(',')] if args.columns else None
        write_markdown_table(md_path, summary_rows, columns=columns)
        print(f'Wrote markdown table: {md_path}')

    # Write JSON (optional)
    if args.json_out:
        json_path = os.path.join(dir_path, args.json_out) if not os.path.isabs(args.json_out) else args.json_out
        with open(json_path, 'w', encoding='utf-8') as jf:
            json.dump(summary_rows, jf, indent=2)
        print(f'Wrote JSON: {json_path}')

    return 0


if __name__ == '__main__':
    raise SystemExit(main())