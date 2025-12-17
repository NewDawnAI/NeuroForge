#!/usr/bin/env python3
"""
Generate per-tag self-model summary JSON files for the dashboard.

Inputs (best-effort, all optional):
- --csv-dir: directory to search for self_model CSVs (e.g., Artifacts/CSV)
  Expected pattern: files containing 'self_model' in name.
  CSV rows may contain a JSON-like field with awareness/confidence metrics.
- --json-dir: directory containing JSON self-model events (e.g., Artifacts/JSON/self_model)
  Expected: either per-run JSON files or a single file with a list of events.
- --tag-map: path to tag_map.json mapping run labels -> tags (string or list)
- --label-map: optional label_map.json to alias raw run suffixes
- --out-root: output root directory for per-tag files (e.g., Artifacts/CSV/tags)

Outputs:
- For each tag, writes out "<out-root>/<safe-tag>/self_model_summary.json" as an
  array of objects with keys: awareness, confidence, identity_similarity, self_model_drift.

Notes:
- If no source self-model data is found for a tag, no output file is written for that tag.
- Confidence is derived from fields if possible: prefer 'confidence'; else average of
  'cognitive_conf' and 'emotional_conf' if present.
- identity_similarity is extracted from numeric columns or JSON; if baseline is available,
  drift is computed as value - baseline (first non-null value); else drift omitted.
- Script is resilient: missing files cause a skip, not failure.
"""
import os
import json
import argparse
import csv
from typing import List, Dict, Any


def load_json(path: str) -> Any:
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception:
        return None


def discover_self_model_csv(csv_dir: str) -> List[str]:
    out = []
    if os.path.isdir(csv_dir):
        for name in os.listdir(csv_dir):
            if 'self_model' in name and name.endswith('.csv'):
                out.append(os.path.join(csv_dir, name))
    return out


def parse_self_model_csv(path: str) -> List[Dict[str, Any]]:
    rows = []
    try:
        with open(path, 'r', encoding='utf-8') as f:
            reader = csv.reader(f)
            for row in reader:
                if not row:
                    continue
                # Heuristic parse based on observed exports
                # Common layout: id, run_id, ts_ms, step, json_blob, maybe numeric identity_similarity
                awareness = None
                confidence = None
                identity_similarity = None
                drift = None
                # Attempt to parse JSON blob in any field
                blob = None
                for cell in row:
                    cell_s = str(cell).strip()
                    if ('{"' in cell_s or "{'" in cell_s) and ('awareness' in cell_s or 'confidence' in cell_s or 'cognitive_conf' in cell_s):
                        try:
                            blob = json.loads(cell_s.replace("'", '"'))
                        except Exception:
                            blob = None
                        break
                if blob and isinstance(blob, dict):
                    awareness = blob.get('awareness')
                    # Prefer unified 'confidence'; fall back to average of components
                    if 'confidence' in blob and isinstance(blob['confidence'], (int, float)):
                        confidence = float(blob['confidence'])
                    else:
                        cc = blob.get('cognitive_conf')
                        ec = blob.get('emotional_conf')
                        if isinstance(cc, (int, float)) and isinstance(ec, (int, float)):
                            confidence = (float(cc) + float(ec)) / 2.0
                        elif isinstance(cc, (int, float)):
                            confidence = float(cc)
                        elif isinstance(ec, (int, float)):
                            confidence = float(ec)
                # Try to find a standalone numeric that could be identity_similarity
                for cell in row[::-1]:  # search from end
                    try:
                        num = float(cell)
                        identity_similarity = num
                        break
                    except Exception:
                        continue
                # Only include row if we have at least one metric
                if awareness is not None or confidence is not None or identity_similarity is not None:
                    rows.append({
                        'awareness': awareness,
                        'confidence': confidence,
                        'identity_similarity': identity_similarity,
                        # drift to compute later once we have baseline
                    })
    except Exception:
        pass
    # compute drift vs first identity_similarity if available
    baseline = None
    for r in rows:
        v = r.get('identity_similarity')
        if isinstance(v, (int, float)):
            baseline = float(v)
            break
    if baseline is not None:
        for r in rows:
            v = r.get('identity_similarity')
            if isinstance(v, (int, float)):
                r['self_model_drift'] = float(v) - baseline
    return rows


def main():
    ap = argparse.ArgumentParser(description='Generate per-tag self-model summary JSON files.')
    ap.add_argument('--csv-dir', default='Artifacts/CSV', help='Directory to scan for self_model CSVs')
    ap.add_argument('--json-dir', default='Artifacts/JSON/self_model', help='Directory containing self_model JSON files (optional)')
    ap.add_argument('--tag-map', default='Artifacts/CSV/tag_map.json', help='Path to tag_map.json (run label -> tags)')
    ap.add_argument('--label-map', default='Artifacts/CSV/label_map.json', help='Optional label_map.json for label aliasing')
    ap.add_argument('--out-root', default='Artifacts/CSV/tags', help='Output root directory for per-tag summary JSONs')
    args = ap.parse_args()

    os.makedirs(args.out_root, exist_ok=True)
    tag_map = load_json(args.tag_map) or {}
    label_map = load_json(args.label_map) or {}

    # Build tag set
    tag_set = set()
    if isinstance(tag_map, dict):
        for k, v in tag_map.items():
            if isinstance(v, list):
                for t in v:
                    t = str(t).strip()
                    if t:
                        tag_set.add(t)
            elif isinstance(v, str):
                t = v.strip()
                if t:
                    tag_set.add(t)
    tags = sorted(tag_set)

    # Aggregate available self_model data globally (no strong run-label mapping yet)
    csv_files = discover_self_model_csv(args.csv_dir)
    # Parse all CSVs; we'll reuse the same rows for all tags until per-run mapping is available
    all_rows = []
    for p in csv_files:
        rows = parse_self_model_csv(p)
        if rows:
            all_rows.extend(rows)
    # Also parse JSON files if present (best-effort: expect list of dicts with keys we need)
    if os.path.isdir(args.json_dir):
        for name in os.listdir(args.json_dir):
            if name.lower().endswith('.json'):
                data = load_json(os.path.join(args.json_dir, name))
                if isinstance(data, list):
                    for obj in data:
                        if isinstance(obj, dict):
                            r = {
                                'awareness': obj.get('awareness'),
                                'confidence': obj.get('confidence'),
                                'identity_similarity': obj.get('identity_similarity'),
                            }
                            all_rows.append(r)
    # Compute global baseline drift once (per-tag drift will be identical for now)
    baseline = None
    for r in all_rows:
        v = r.get('identity_similarity')
        if isinstance(v, (int, float)):
            baseline = float(v)
            break
    if baseline is not None:
        for r in all_rows:
            v = r.get('identity_similarity')
            if isinstance(v, (int, float)):
                r['self_model_drift'] = float(v) - baseline

    # Write per-tag files only if we have data
    for t in tags:
        safe = ''.join([c if c.isalnum() or c in '._-' else '-' for c in t])
        out_dir = os.path.join(args.out_root, safe)
        os.makedirs(out_dir, exist_ok=True)
        if all_rows:
            out_path = os.path.join(out_dir, 'self_model_summary.json')
            with open(out_path, 'w', encoding='utf-8') as f:
                json.dump(all_rows, f, indent=2)
            print(f'Wrote {out_path} ({len(all_rows)} rows)')
        else:
            # no data -> skip writing, dashboard remains silent for this tag
            print(f'No self-model data found; skipping tag {t}')

    print('Done.')


if __name__ == '__main__':
    raise SystemExit(main())