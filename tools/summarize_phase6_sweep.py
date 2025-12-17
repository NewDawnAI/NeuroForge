import argparse
import csv
import glob
import json
import math
import os
from datetime import datetime
from statistics import mean, pstdev
from typing import List, Dict, Any

# Helper functions

def pearson_corr(xs: List[float], ys: List[float]):
    n = min(len(xs), len(ys))
    if n < 2:
        return None
    xs = xs[:n]
    ys = ys[:n]
    mx = sum(xs) / n
    my = sum(ys) / n
    sum_cov = 0.0
    sx = 0.0
    sy = 0.0
    for i in range(n):
        dx = xs[i] - mx
        dy = ys[i] - my
        sum_cov += dx * dy
        sx += dx * dx
        sy += dy * dy
    if sx == 0 or sy == 0:
        return None
    return sum_cov / (math.sqrt(sx) * math.sqrt(sy))


def parse_jsonl(path: str) -> List[Dict[str, Any]]:
    events = []
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError:
                # Skip malformed lines
                continue
    return events


def extract_metrics_for_file(path: str) -> Dict[str, Any]:
    evs = parse_jsonl(path)
    # Initialize
    reward_vals: List[float] = []
    episode_returns: List[float] = []
    gate_events: List[Dict[str, Any]] = []
    drift_vals: List[float] = []

    for e in evs:
        try:
            event = e.get('event') or e.get('payload', {}).get('event')
            phase = e.get('phase')
            payload = e.get('payload', {})
            if event == 'reward':
                rv = payload.get('reward')
                if isinstance(rv, (int, float)):
                    reward_vals.append(float(rv))
            elif event == 'episode_end':
                ret = payload.get('return')
                if isinstance(ret, (int, float)):
                    episode_returns.append(float(ret))
            elif phase == '6' and event == 'gate':
                gate_events.append(e)
            elif event == 'drift':
                dv = payload.get('drift')
                if isinstance(dv, (int, float)):
                    drift_vals.append(float(dv))
        except Exception:
            continue

    gates = len(gate_events)
    overrides = sum(1 for g in gate_events if g.get('payload', {}).get('override_applied') is True)
    contradictions_total = 0
    contradictions_nonoverride = 0
    policy_scores: List[float] = []
    phase6_scores: List[float] = []

    for g in gate_events:
        pl = g.get('payload', {})
        pa = pl.get('policy_action')
        p6a = pl.get('phase6_action')
        if isinstance(pa, int) and isinstance(p6a, int):
            if pa != p6a:
                contradictions_total += 1
                if not pl.get('override_applied'):
                    contradictions_nonoverride += 1
        ps = pl.get('policy_score')
        p6s = pl.get('phase6_score')
        if isinstance(ps, (int, float)):
            policy_scores.append(float(ps))
        if isinstance(p6s, (int, float)):
            phase6_scores.append(float(p6s))

    # Metrics
    avg_reward = mean(reward_vals) if reward_vals else None
    episode_mean_reward = mean(episode_returns) if episode_returns else None
    episode_reward_std = pstdev(episode_returns) if len(episode_returns) > 1 else (0.0 if episode_returns else None)
    override_rate = (overrides / gates) if gates > 0 else 0.0
    contradiction_rate_total = (contradictions_total / gates) if gates > 0 else 0.0
    contradiction_rate_nonoverride = (contradictions_nonoverride / gates) if gates > 0 else 0.0

    corr_confidence_reward = pearson_corr(phase6_scores, reward_vals)

    drift_triggers = sum(1 for d in drift_vals if d > 0.25)
    # Use reward count as proxy for steps
    steps_proxy = len(reward_vals)
    drift_trigger_rate = (drift_triggers / steps_proxy) if steps_proxy > 0 else 0.0

    # Infer margin from filename like phase6_mXX.jsonl
    base = os.path.basename(path)
    margin = None
    try:
        if 'phase6_m' in base:
            idx = base.index('phase6_m') + len('phase6_m')
            mm = base[idx:idx+2]
            if mm.isdigit():
                margin = int(mm) / 100.0
    except Exception:
        pass

    return {
        'file': base,
        'margin': margin,
        'lines': len(evs),
        'episodes_start': sum(1 for e in evs if e.get('event') == 'episode_start'),
        'episodes_end': sum(1 for e in evs if e.get('event') == 'episode_end'),
        'gates': gates,
        'overrides': overrides,
        'override_rate': override_rate,
        'contradiction_rate_total': contradiction_rate_total,
        'contradiction_rate_nonoverride': contradiction_rate_nonoverride,
        'avg_reward': avg_reward,
        'episode_mean_reward': episode_mean_reward,
        'episode_reward_std': episode_reward_std,
        'corr_confidence_reward': corr_confidence_reward,
        'drift_trigger_rate': drift_trigger_rate,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--inputs', required=True, help='Glob pattern for input JSONL files (e.g., phase6_m*.jsonl)')
    ap.add_argument('--out', required=True, help='Output CSV path')
    ap.add_argument('--json', help='Optional JSON output path (default: same dir, .json)')
    ap.add_argument('--date', help='Date for this sweep run (YYYY-MM-DD format, default: today)')
    args = ap.parse_args()
    
    # Parse or default the date
    if args.date:
        try:
            sweep_date = datetime.strptime(args.date, '%Y-%m-%d').strftime('%Y-%m-%d')
        except ValueError:
            raise SystemExit(f'Invalid date format: {args.date}. Use YYYY-MM-DD.')
    else:
        sweep_date = datetime.now().strftime('%Y-%m-%d')

    files = sorted(glob.glob(args.inputs))
    if not files:
        raise SystemExit(f'No inputs match: {args.inputs}')

    # Baseline avg reward (from phase6_m00.jsonl or gate_baseline.jsonl)
    baseline_avg = None
    for cand in ['phase6_m00.jsonl', 'gate_baseline.jsonl']:
        if os.path.exists(cand):
            evs = parse_jsonl(cand)
            rewards = []
            for e in evs:
                if e.get('event') == 'reward':
                    rv = e.get('payload', {}).get('reward')
                    if isinstance(rv, (int, float)):
                        rewards.append(float(rv))
            if rewards:
                baseline_avg = mean(rewards)
                break

    rows = []
    for f in files:
        m = extract_metrics_for_file(f)
        if baseline_avg is not None and m['avg_reward'] is not None:
            m['delta_reward_vs_baseline'] = m['avg_reward'] - baseline_avg
        else:
            m['delta_reward_vs_baseline'] = None
        # Add date metadata
        m['date'] = sweep_date
        rows.append(m)

    # Write CSV
    out_csv = args.out
    os.makedirs(os.path.dirname(out_csv), exist_ok=True) if os.path.dirname(out_csv) else None
    fieldnames = [
        'date',
        'margin',
        'override_rate',
        'avg_reward',
        'episode_mean_reward',
        'episode_reward_std',
        'contradiction_rate_total',
        'contradiction_rate_nonoverride',
        'corr_confidence_reward',
        'drift_trigger_rate',
        'delta_reward_vs_baseline',
        'gates',
        'overrides',
        'file',
        'lines',
        'episodes_start',
        'episodes_end',
    ]
    with open(out_csv, 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        for r in rows:
            # Format None -> empty string
            rr = {k: ('' if r.get(k) is None else r.get(k)) for k in fieldnames}
            w.writerow(rr)

    # Write JSON companion
    out_json = args.json or (os.path.splitext(out_csv)[0] + '.json')
    with open(out_json, 'w', encoding='utf-8') as jf:
        json.dump(rows, jf, indent=2)

    print(f'Wrote CSV: {out_csv}')
    print(f'Wrote JSON: {out_json}')


if __name__ == '__main__':
    main()