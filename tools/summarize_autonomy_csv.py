#!/usr/bin/env python3
import argparse
import csv
import os
import glob
import statistics

def read_spike_metrics(spike_path):
    try:
        with open(spike_path, 'r', newline='') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
            spike_rows = len(rows)
            unique_neurons = len({r.get('neuron_id') for r in rows if 'neuron_id' in r})
            return spike_rows, unique_neurons
    except FileNotFoundError:
        return 0, 0

def read_synapse_metrics(syn_path):
    try:
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
            else:
                w_min = w_max = w_mean = 0.0
            return syn_rows, w_min, w_max, w_mean
    except FileNotFoundError:
        return 0, 0.0, 0.0, 0.0


def pair_files(dir_path):
    syn_files = sorted(glob.glob(os.path.join(dir_path, 'autonomy_synapses*.csv')))
    spike_files = sorted(glob.glob(os.path.join(dir_path, 'autonomy_spikes*.csv')))
    spike_map = {os.path.basename(p).replace('autonomy_spikes', ''): p for p in spike_files}
    pairs = []
    for syn in syn_files:
        suffix = os.path.basename(syn).replace('autonomy_synapses', '')
        spike = spike_map.get(suffix)
        pairs.append((syn, spike))
    return pairs


def main():
    ap = argparse.ArgumentParser(description='Summarize autonomy sweep CSVs (synapses + spikes).')
    ap.add_argument('--dir', default='.', help='Directory containing autonomy_* CSVs (default: .)')
    ap.add_argument('--out', default='autonomy_sweep_summary.csv', help='Output summary CSV file')
    args = ap.parse_args()

    dir_path = os.path.abspath(args.dir)
    pairs = pair_files(dir_path)
    if not pairs:
        print(f'No autonomy_* CSVs found in {dir_path}')
        return 1

    out_path = os.path.join(dir_path, args.out) if not os.path.isabs(args.out) else args.out
    fieldnames = [
        'run_label', 'synapses_file', 'spikes_file', 'synapse_rows',
        'weight_min', 'weight_max', 'weight_mean', 'spike_rows', 'unique_spiking_neurons'
    ]

    with open(out_path, 'w', newline='') as fout:
        writer = csv.DictWriter(fout, fieldnames=fieldnames)
        writer.writeheader()
        for syn_path, spike_path in pairs:
            run_label = os.path.basename(syn_path).replace('autonomy_synapses', '').replace('.csv', '').strip('_')
            syn_rows, w_min, w_max, w_mean = read_synapse_metrics(syn_path)
            spike_rows, unique_neurons = read_spike_metrics(spike_path) if spike_path else (0, 0)
            writer.writerow({
                'run_label': run_label or '(unknown)',
                'synapses_file': os.path.basename(syn_path),
                'spikes_file': os.path.basename(spike_path) if spike_path else '',
                'synapse_rows': syn_rows,
                'weight_min': f'{w_min:.6g}',
                'weight_max': f'{w_max:.6g}',
                'weight_mean': f'{w_mean:.6g}',
                'spike_rows': spike_rows,
                'unique_spiking_neurons': unique_neurons,
            })

    print(f'Wrote summary: {out_path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())