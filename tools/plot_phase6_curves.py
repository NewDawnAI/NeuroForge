#!/usr/bin/env python3
"""
Plot Phase 6 curves directly from phasec_runner.db and summary JSONs.

Generates three figures:
 1) confidence_vs_episode.png        (Self-model confidence per episode)
 2) drift_vs_reward.png              (Scatter: self-model drift vs episode reward ratio)
 3) contradiction_rate_vs_conf.png   (Scatter: contradiction rate vs option confidence)

Default inputs:
 - DB:                ./phasec_runner.db
 - Self summary JSON: ./PhaseC_Logs/runner/self_model_summary.json
 - Options summary:   ./PhaseC_Logs/runner/options_summary.json
 - Output directory:  ./PhaseC_Logs/figures

Usage:
    python tools/plot_phase6_curves.py \
        --db phasec_runner.db \
        --self-json PhaseC_Logs/runner/self_model_summary.json \
        --options-json PhaseC_Logs/runner/options_summary.json \
        --outdir PhaseC_Logs/figures

Notes:
 - Contradiction rate is computed per episode by parsing inferred_facts.fact_json
   to obtain the episode for each fact, and then joining verifications on fact_id.
 - Option confidence for the contradiction scatter is sourced from options_summary.json.
"""
import argparse
import json
import os
import sqlite3
from collections import defaultdict

import matplotlib.pyplot as plt


def ensure_dir(path: str):
    os.makedirs(path, exist_ok=True)


def load_json(path: str):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def parse_args():
    ap = argparse.ArgumentParser(description='Plot Phase 6 curves from DB and summaries')
    ap.add_argument('--db', default='phasec_runner.db', help='Path to SQLite DB (default: phasec_runner.db)')
    ap.add_argument('--self-json', default=os.path.join('PhaseC_Logs', 'runner', 'self_model_summary.json'),
                    help='Path to self_model_summary.json')
    ap.add_argument('--options-json', default=os.path.join('PhaseC_Logs', 'runner', 'options_summary.json'),
                    help='Path to options_summary.json')
    ap.add_argument('--outdir', default=os.path.join('PhaseC_Logs', 'figures'), help='Output directory for figures')
    return ap.parse_args()


def get_episode_confidence(self_summary: dict):
    episodes = self_summary.get('episodes', [])
    # Return sorted pairs (episode, confidence)
    data = [(int(ep.get('episode', i)), float(ep.get('confidence', 0.0))) for i, ep in enumerate(episodes)]
    data.sort(key=lambda x: x[0])
    return data


def get_drift_reward(self_summary: dict, options_summary: dict):
    sm_eps = {int(ep.get('episode', i)): float(ep.get('self_model_drift', 0.0))
              for i, ep in enumerate(self_summary.get('episodes', []))}
    opt_eps = {int(ep.get('episode', i)): float(ep.get('reward', 0.0))
               for i, ep in enumerate(options_summary.get('episodes', []))}
    common = sorted(set(sm_eps.keys()) & set(opt_eps.keys()))
    return [(ep, sm_eps[ep], opt_eps[ep]) for ep in common]


def compute_contradiction_rate_vs_conf(db_path: str, options_summary: dict, run_id_from_json: int):
    """
    Returns list of tuples: (episode, contradiction_rate, option_confidence)
    - contradiction_rate computed from verifications joined via inferred_facts.fact_json->episode.
    - option_confidence sourced from options_summary.json.
    """
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    # Map fact_id -> episode, for the given run_id
    # inferred_facts columns assumed: id (PK), ts, fact_json (TEXT), confidence, run_id, ...
    # We specifically parse 'fact_json' to extract 'episode'.
    cur.execute("SELECT id, fact_json FROM inferred_facts WHERE run_id = ?", (run_id_from_json,))
    fact_id_to_episode = {}
    for fid, fjson in cur.fetchall():
        try:
            data = json.loads(fjson) if fjson and fjson.strip().startswith('{') else None
        except Exception:
            data = None
        if isinstance(data, dict) and 'episode' in data:
            try:
                ep = int(data.get('episode'))
                fact_id_to_episode[fid] = ep
            except Exception:
                continue

    # Collect verifications and aggregate per episode
    # verifications columns assumed: id, ts, fact_id, type, contradiction, details_json, run_id
    # Treat 'contradiction' as integer 0/1 or boolean.
    cur.execute("SELECT fact_id, contradiction FROM verifications WHERE run_id = ?", (run_id_from_json,))
    epi_counts = defaultdict(lambda: {'total': 0, 'contradictions': 0})
    for fact_id, contradiction in cur.fetchall():
        ep = fact_id_to_episode.get(fact_id)
        if ep is None:
            continue
        epi_counts[ep]['total'] += 1
        try:
            epi_counts[ep]['contradictions'] += int(contradiction)
        except Exception:
            # fallback: truthy -> 1
            epi_counts[ep]['contradictions'] += 1 if contradiction else 0

    conn.close()

    # Option confidence per episode from options_summary.json
    opt_conf = {int(ep.get('episode', i)): float(ep.get('confidence', 0.0))
                for i, ep in enumerate(options_summary.get('episodes', []))}

    # Build results for episodes that have counts
    results = []
    for ep, cnts in epi_counts.items():
        total = cnts['total']
        contradictions = cnts['contradictions']
        rate = (contradictions / total) if total > 0 else 0.0
        conf = opt_conf.get(ep, None)
        if conf is None:
            # skip episodes without an option confidence
            continue
        results.append((ep, rate, conf))
    results.sort(key=lambda x: x[0])
    return results


def plot_confidence_vs_episode(data, outdir):
    episodes = [ep for ep, _ in data]
    confidences = [c for _, c in data]
    plt.figure(figsize=(8, 4.5))
    plt.plot(episodes, confidences, marker='o', color='#1f77b4')
    plt.xlabel('Episode')
    plt.ylabel('Self-model Confidence')
    plt.title('Confidence vs Episode')
    plt.grid(True, alpha=0.3)
    out_path = os.path.join(outdir, 'confidence_vs_episode.png')
    plt.tight_layout()
    plt.savefig(out_path, dpi=140)
    plt.close()
    return out_path


def plot_drift_vs_reward(data, outdir):
    # data: list of (episode, drift, reward)
    drifts = [d for _, d, _ in data]
    rewards = [r for _, _, r in data]
    plt.figure(figsize=(6.5, 4.5))
    plt.scatter(drifts, rewards, color='#ff7f0e', alpha=0.8)
    plt.xlabel('Self-model Drift')
    plt.ylabel('Reward Ratio')
    plt.title('Drift vs Reward')
    plt.grid(True, alpha=0.3)
    out_path = os.path.join(outdir, 'drift_vs_reward.png')
    plt.tight_layout()
    plt.savefig(out_path, dpi=140)
    plt.close()
    return out_path


def plot_contradiction_rate_vs_conf(data, outdir):
    # data: list of (episode, contradiction_rate, option_confidence)
    rates = [rate for _, rate, _ in data]
    confs = [conf for _, _, conf in data]
    plt.figure(figsize=(6.5, 4.5))
    plt.scatter(confs, rates, color='#2ca02c', alpha=0.8)
    plt.xlabel('Option Confidence')
    plt.ylabel('Contradiction Rate')
    plt.title('Contradiction Rate vs Option Confidence')
    plt.grid(True, alpha=0.3)
    out_path = os.path.join(outdir, 'contradiction_rate_vs_conf.png')
    plt.tight_layout()
    plt.savefig(out_path, dpi=140)
    plt.close()
    return out_path


def main():
    args = parse_args()
    ensure_dir(args.outdir)

    # Load summaries
    self_summary = load_json(args.self_json)
    options_summary = load_json(args.options_json)

    # Use run_id embedded in self_summary if present, else 0
    run_id = int(self_summary.get('run_id', options_summary.get('run_id', 0)))

    # Prepare datasets
    conf_ep = get_episode_confidence(self_summary)
    drift_reward = get_drift_reward(self_summary, options_summary)
    contr_conf = compute_contradiction_rate_vs_conf(args.db, options_summary, run_id)

    # Plot and save
    out1 = plot_confidence_vs_episode(conf_ep, args.outdir)
    out2 = plot_drift_vs_reward(drift_reward, args.outdir)
    out3 = plot_contradiction_rate_vs_conf(contr_conf, args.outdir)

    print('Generated plots:')
    print(' -', out1)
    print(' -', out2)
    print(' -', out3)


if __name__ == '__main__':
    main()