#!/usr/bin/env python3
"""
Aggregate stage-level telemetry across runs into a stage_summary.

Inputs:
- --live-json: path to live_summary.json (from summarize_csv.py)
- --out-json: path to write stage_summary.json
- --md-out: optional Markdown table output

Logic:
- Parses each row's 'tags' field (comma-separated). Any token starting with 'stage='
  contributes the run to that stage group.
- Aggregates numeric metrics by stage: mean across runs.
- Outputs JSON array: [{ stage: 'stage=X', run_count: N, <metric>: mean, ... }]

Notes:
- Numeric keys are inferred from the first row and filtered for types float/int.
- Safe for missing or empty inputs; outputs an empty list if no stages found.
"""
import os
import json
import argparse
from typing import List, Dict, Any


def load_json(path: str) -> Any:
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception:
        return None


def is_number(x):
    return isinstance(x, (int, float))


def main():
    ap = argparse.ArgumentParser(description='Aggregate stage-level telemetry from live_summary.json')
    ap.add_argument('--live-json', default='Artifacts/CSV/live_summary.json')
    ap.add_argument('--out-json', default='Artifacts/CSV/stage_summary.json')
    ap.add_argument('--md-out', default=None)
    args = ap.parse_args()

    rows = load_json(args.live_json) or []
    if not isinstance(rows, list) or not rows:
        # write empty output
        os.makedirs(os.path.dirname(args.out_json) or '.', exist_ok=True)
        with open(args.out_json, 'w', encoding='utf-8') as f:
            json.dump([], f, indent=2)
        if args.md_out:
            with open(args.md_out, 'w', encoding='utf-8') as f:
                f.write('# Stage Summary\n\n_No data found._\n')
        print('No live_summary rows found; wrote empty stage summary.')
        return 0

    # Identify numeric keys
    sample = rows[0]
    numeric_keys = [k for k, v in sample.items() if is_number(v)]
    # group by stage
    stages: Dict[str, Dict[str, Any]] = {}
    for r in rows:
        tags_str = r.get('tags') or ''
        tags = [t.strip() for t in tags_str.split(',') if t.strip()]
        st_tokens = [t for t in tags if t.startswith('stage=')]
        if not st_tokens:
            continue
        for st in st_tokens:
            g = stages.setdefault(st, {'stage': st, 'run_count': 0})
            g['run_count'] += 1
            for k in numeric_keys:
                v = r.get(k)
                if is_number(v):
                    g[k] = g.get(k, 0.0) + float(v)
    # compute means
    out_rows: List[Dict[str, Any]] = []
    for st, g in stages.items():
        rc = max(1, int(g['run_count']))
        row = {'stage': st, 'run_count': rc}
        for k in numeric_keys:
            if k in g:
                row[k] = g[k] / rc
        out_rows.append(row)

    # write JSON
    os.makedirs(os.path.dirname(args.out_json) or '.', exist_ok=True)
    with open(args.out_json, 'w', encoding='utf-8') as f:
        json.dump(out_rows, f, indent=2)
    print(f'Wrote {args.out_json} ({len(out_rows)} stages)')

    # optional Markdown
    if args.md_out:
        lines = ['# Stage Summary', '', '| Stage | Runs | ' + ' | '.join(numeric_keys) + ' |', '|---|---|' + ('---|' * len(numeric_keys))]
        for r in out_rows:
            vals = [f"{r.get(k, '')}" for k in numeric_keys]
            lines.append('| ' + r['stage'] + ' | ' + str(r['run_count']) + ' | ' + ' | '.join(vals) + ' |')
        with open(args.md_out, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines) + '\n')
        print(f'Wrote {args.md_out}')

    return 0


if __name__ == '__main__':
    raise SystemExit(main())