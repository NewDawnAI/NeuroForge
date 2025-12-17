#!/usr/bin/env python3
"""
Summarize Phase C runner outputs into per-episode aggregates and global stats.

Inputs:
 - --self-model: Path to self_model_summary.json
 - --options:    Path to options_summary.json
 - --out:        Output JSON path (default: PhaseC_Logs/runner/phasec_runner_summary.json)
 - --md-out:     Output Markdown path (default: PhaseC_Logs/runner/phasec_runner_summary.md)

Outputs:
 - JSON: episode-wise aggregates and global statistics including correlations
 - Markdown: human-readable summary table and correlation matrix

Notes:
 - Contradiction rate is derived from options_summary if available.
   Expected keys per episode: 'contradiction_rate' or ('contradictions', 'verifications').
 - Confidence is taken from self-model if present, else falls back to options.
 - Drift comes from self_model_summary episodes under 'self_model_drift'.
 - Reward comes from options_summary episodes under 'reward'.
"""
import argparse
import json
import os
from statistics import mean, variance


def load_json(path: str):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def safe_float(x, default=None):
    try:
        return float(x)
    except Exception:
        return default


def parse_args():
    ap = argparse.ArgumentParser(description='Summarize Phase C runner outputs for dashboard')
    ap.add_argument('--self-model', dest='self_model', default=os.path.join('PhaseC_Logs', 'runner', 'self_model_summary.json'),
                    help='Path to self_model_summary.json')
    ap.add_argument('--options', default=os.path.join('PhaseC_Logs', 'runner', 'options_summary.json'),
                    help='Path to options_summary.json')
    ap.add_argument('--out', default=os.path.join('PhaseC_Logs', 'runner', 'phasec_runner_summary.json'),
                    help='Output JSON path')
    ap.add_argument('--md-out', dest='md_out', default=os.path.join('PhaseC_Logs', 'runner', 'phasec_runner_summary.md'),
                    help='Output Markdown path')
    return ap.parse_args()


def pearson_corr(xs, ys):
    # Filter None values
    pairs = [(x, y) for x, y in zip(xs, ys) if x is not None and y is not None]
    if len(pairs) < 2:
        return None
    xs_f = [x for x, _ in pairs]
    ys_f = [y for _, y in pairs]
    mx = mean(xs_f)
    my = mean(ys_f)
    num = sum((x - mx) * (y - my) for x, y in pairs)
    den_x = sum((x - mx) ** 2 for x in xs_f)
    den_y = sum((y - my) ** 2 for y in ys_f)
    if den_x <= 0 or den_y <= 0:
        return None
    return num / (den_x ** 0.5 * den_y ** 0.5)


def summarize(self_summary: dict, options_summary: dict):
    # Build per-episode maps
    sm_eps = {int(ep.get('episode', i)): {
                    'confidence': safe_float(ep.get('confidence')), 
                    'drift': safe_float(ep.get('self_model_drift'))
                }
              for i, ep in enumerate(self_summary.get('episodes', []))}

    opt_eps = {int(ep.get('episode', i)): {
                    'reward': safe_float(ep.get('reward')), 
                    'confidence': safe_float(ep.get('confidence')),
                    'contradiction_rate': (
                        safe_float(ep.get('contradiction_rate'))
                        if ep.get('contradiction_rate') is not None
                        else (safe_float(ep.get('contradictions'), 0.0) / safe_float(ep.get('verifications'), None)
                              if ep.get('contradictions') is not None and ep.get('verifications') not in (None, 0)
                              else None)
                    )
                }
              for i, ep in enumerate(options_summary.get('episodes', []))}

    episodes = sorted(set(sm_eps.keys()) | set(opt_eps.keys()))
    per_episode = []

    # Arrays for global stats
    arr_conf = []
    arr_reward = []
    arr_drift = []
    arr_contra = []

    for ep in episodes:
        sm = sm_eps.get(ep, {})
        opt = opt_eps.get(ep, {})
        confidence = sm.get('confidence', None)
        if confidence is None:
            confidence = opt.get('confidence', None)
        drift = sm.get('drift', None)
        reward = opt.get('reward', None)
        contradiction_rate = opt.get('contradiction_rate', None)

        per_episode.append({
            'episode': ep,
            'avg_confidence': confidence,
            'avg_reward': reward,
            'avg_drift': drift,
            'contradiction_rate': contradiction_rate,
        })

        arr_conf.append(confidence)
        arr_reward.append(reward)
        arr_drift.append(drift)
        arr_contra.append(contradiction_rate)

    # Global stats (mean/variance over non-None)
    def mean_var(arr):
        vals = [x for x in arr if x is not None]
        if len(vals) == 0:
            return None, None
        if len(vals) == 1:
            return vals[0], 0.0
        return mean(vals), variance(vals)

    m_conf, v_conf = mean_var(arr_conf)
    m_reward, v_reward = mean_var(arr_reward)
    m_drift, v_drift = mean_var(arr_drift)
    m_contra, v_contra = mean_var(arr_contra)

    # Correlations
    corr_conf_reward = pearson_corr(arr_conf, arr_reward)
    corr_conf_drift = pearson_corr(arr_conf, arr_drift)
    corr_reward_drift = pearson_corr(arr_reward, arr_drift)
    corr_contra_conf = pearson_corr(arr_contra, arr_conf)
    corr_contra_reward = pearson_corr(arr_contra, arr_reward)
    corr_contra_drift = pearson_corr(arr_contra, arr_drift)

    summary = {
        'run_id': self_summary.get('run_id', options_summary.get('run_id')),
        'episodes_count': len(episodes),
        'episodes': per_episode,
        'global_stats': {
            'mean_confidence': m_conf,
            'var_confidence': v_conf,
            'mean_reward': m_reward,
            'var_reward': v_reward,
            'mean_drift': m_drift,
            'var_drift': v_drift,
            'mean_contradiction_rate': m_contra,
            'var_contradiction_rate': v_contra,
        },
        'correlations': {
            'confidence_reward': corr_conf_reward,
            'confidence_drift': corr_conf_drift,
            'reward_drift': corr_reward_drift,
            'contradiction_confidence': corr_contra_conf,
            'contradiction_reward': corr_contra_reward,
            'contradiction_drift': corr_contra_drift,
        }
    }
    return summary


def to_markdown(summary: dict) -> str:
    lines = []
    lines.append(f"# Phase C Runner Summary (run_id={summary.get('run_id')})")
    lines.append("")
    lines.append(f"Episodes: {summary.get('episodes_count')}")
    lines.append("")

    # Global stats
    gs = summary['global_stats']
    lines.append("## Global Stats")
    lines.append("- mean_confidence: " + str(gs['mean_confidence']))
    lines.append("- var_confidence: " + str(gs['var_confidence']))
    lines.append("- mean_reward: " + str(gs['mean_reward']))
    lines.append("- var_reward: " + str(gs['var_reward']))
    lines.append("- mean_drift: " + str(gs['mean_drift']))
    lines.append("- var_drift: " + str(gs['var_drift']))
    lines.append("- mean_contradiction_rate: " + str(gs['mean_contradiction_rate']))
    lines.append("- var_contradiction_rate: " + str(gs['var_contradiction_rate']))
    lines.append("")

    # Correlations
    corr = summary['correlations']
    lines.append("## Correlations (Pearson)")
    lines.append("- confidence ↔ reward: " + str(corr['confidence_reward']))
    lines.append("- confidence ↔ drift: " + str(corr['confidence_drift']))
    lines.append("- reward ↔ drift: " + str(corr['reward_drift']))
    lines.append("- contradiction ↔ confidence: " + str(corr['contradiction_confidence']))
    lines.append("- contradiction ↔ reward: " + str(corr['contradiction_reward']))
    lines.append("- contradiction ↔ drift: " + str(corr['contradiction_drift']))
    lines.append("")

    # Episode table
    lines.append("## Per-Episode Aggregates")
    lines.append("Episode | avg_confidence | avg_reward | avg_drift | contradiction_rate")
    lines.append("--- | --- | --- | --- | ---")
    for ep in summary['episodes']:
        lines.append(f"{ep['episode']} | {ep['avg_confidence']} | {ep['avg_reward']} | {ep['avg_drift']} | {ep['contradiction_rate']}")
    lines.append("")
    return "\n".join(lines)


def ensure_parent_dir(path: str):
    parent = os.path.dirname(os.path.abspath(path))
    os.makedirs(parent, exist_ok=True)


def main():
    args = parse_args()
    self_summary = load_json(args.self_model)
    options_summary = load_json(args.options)
    summary = summarize(self_summary, options_summary)

    # Write JSON
    ensure_parent_dir(args.out)
    with open(args.out, 'w', encoding='utf-8') as f:
        json.dump(summary, f, indent=2)

    # Write Markdown
    ensure_parent_dir(args.md_out)
    md = to_markdown(summary)
    with open(args.md_out, 'w', encoding='utf-8') as f:
        f.write(md)

    print('Wrote:', args.out)
    print('Wrote:', args.md_out)


if __name__ == '__main__':
    main()