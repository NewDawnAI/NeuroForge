import argparse
import json
import os
import csv

def load_meta(meta_path: str):
    with open(meta_path, 'r', encoding='utf-8') as f:
        return json.load(f)

def write_couplings_csv(meta: dict, out_dir: str):
    context = meta.get('context', {})
    couplings = context.get('couplings', []) or []
    kappa = context.get('kappa', None)
    enabled = context.get('couplings_enabled', False)
    label = context.get('label', None)
    gain = context.get('gain', None)
    window = context.get('window', None)
    run_id = meta.get('run_id', meta.get('run', {}).get('id'))

    if run_id is None:
        # Fallback: try generic id field or default to 0
        run_id = meta.get('id', 0)

    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, f"couplings_run_{run_id}.csv")

    with open(out_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['run_id', 'src', 'dst', 'weight', 'kappa', 'couplings_enabled', 'label', 'gain', 'window'])
        for edge in couplings:
            src = edge.get('src')
            dst = edge.get('dst')
            weight = edge.get('weight')
            writer.writerow([run_id, src, dst, weight, kappa, enabled, label, gain, window])

    return out_path

def main():
    parser = argparse.ArgumentParser(description='Export couplings.csv from run_meta.json')
    parser.add_argument('--meta', required=True, help='Path to run_meta.json (from exports or run directory)')
    parser.add_argument('--out-dir', required=True, help='Output directory for couplings CSV')
    args = parser.parse_args()

    meta = load_meta(args.meta)
    out_path = write_couplings_csv(meta, args.out_dir)
    print(f"Wrote: {out_path}")

if __name__ == '__main__':
    main()

