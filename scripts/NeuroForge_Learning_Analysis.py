#!/usr/bin/env python3
import argparse
import os
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

plt.rcParams.update({
    'figure.figsize': (10, 6),
    'axes.grid': True,
    'font.size': 11
})


def ensure_dir(path: str):
    os.makedirs(path, exist_ok=True)


def save_plot(out_dir: str, name: str):
    ensure_dir(out_dir)
    path = os.path.join(out_dir, name)
    plt.tight_layout()
    plt.savefig(path)
    plt.close()
    print(f"Saved figure: {path}")


def align_series(learning_df: pd.DataFrame, reward_df: pd.DataFrame) -> pd.DataFrame:
    """Align avg_weight_change and reward_events by step for correlation and overlays."""
    # Steps from learning_df; average weight change per step
    wstep = learning_df.groupby('step', as_index=False)['avg_weight_change'].mean()
    # Reward events per step (count)
    rstep = pd.DataFrame({'step': [], 'reward_events': []})
    if reward_df is not None and len(reward_df) > 0:
        rstep = reward_df.groupby('step', as_index=False)['reward'].count().rename(columns={'reward': 'reward_events'})
    # Merge and fill missing reward with zeros
    merged = pd.merge(wstep, rstep, on='step', how='left').fillna({'reward_events': 0})
    merged.sort_values('step', inplace=True)
    return merged


def rolling_corr(x: np.ndarray, y: np.ndarray, window: int) -> list:
    """Compute rolling Pearson correlation over a sliding window."""
    out = []
    for i in range(len(x)):
        s = max(0, i - window + 1)
        xi = x[s:i + 1]
        yi = y[s:i + 1]
        if len(xi) >= 2 and np.std(xi) > 0 and np.std(yi) > 0:
            r = np.corrcoef(xi, yi)[0, 1]
        else:
            r = np.nan
        out.append(r)
    return out


def lag_corr(x: np.ndarray, y: np.ndarray, max_lag: int):
    """Compute Pearson correlation across integer lags in [-max_lag, max_lag]."""
    lags = list(range(-max_lag, max_lag + 1))
    vals = []
    for L in lags:
        if L >= 0:
            xs = x[L:]
            ys = y[:len(xs)]
        else:
            ys = y[-L:]
            xs = x[:len(ys)]
        if len(xs) >= 2 and np.std(xs) > 0 and np.std(ys) > 0:
            vals.append(np.corrcoef(xs, ys)[0, 1])
        else:
            vals.append(np.nan)
    return lags, vals


def plot_learning_deltas(learning_df: pd.DataFrame, out_dir: str, reward_df: pd.DataFrame = None, window: int | None = None):
    df = learning_df.sort_values('step')
    plt.figure()
    ax1 = plt.gca()
    ax1.plot(df['step'], df['avg_weight_change'], label='Avg Δw', color='tab:blue')
    ax1.set_xlabel('Step')
    ax1.set_ylabel('Avg Δw')
    ax1.set_title('Learning Dynamics: Avg Δw vs Step')
    # Optional reward overlay on fig1
    if reward_df is not None:
        merged = align_series(learning_df, reward_df)
        r = merged['reward_events'].astype(float)
        if window and window > 1:
            r = r.rolling(window, min_periods=1).mean()
        ax2 = ax1.twinx()
        ax2.bar(merged['step'], r, label='Reward (smoothed)' if window and window > 1 else 'Reward', color='tab:orange', alpha=0.25)
        ax2.set_ylabel('Reward events')
    save_plot(out_dir, 'learning_delta_w_vs_time.png')


def plot_ltp_ltd_ratio(learning_df: pd.DataFrame, out_dir: str):
    df = learning_df.sort_values('step').copy()
    # Avoid division by zero
    df['ltd_safe'] = df['depressed_synapses'].replace(0, pd.NA).fillna(1)
    df['ltp_ltd_ratio'] = df['potentiated_synapses'] / df['ltd_safe']
    plt.figure()
    plt.plot(df['step'], df['ltp_ltd_ratio'], label='LTP/LTD ratio', color='tab:green')
    plt.xlabel('Step')
    plt.ylabel('Ratio')
    plt.title('Synapse Potentiation vs Depression Ratio')
    save_plot(out_dir, 'ltp_ltd_ratio.png')


def plot_reward_coupling_extended(learning_df: pd.DataFrame, reward_df: pd.DataFrame, out_dir: str, window: int = 10, max_lag: int = 20):
    if reward_df is None or len(reward_df) == 0:
        return None
    merged = align_series(learning_df, reward_df)
    steps = merged['step'].values
    w = merged['avg_weight_change'].fillna(0).values
    r = merged['reward_events'].astype(float).values
    r_smooth = pd.Series(r).rolling(window, min_periods=1).mean().values if window and window > 1 else r
    corr_series = rolling_corr(w, r_smooth, window if window and window > 1 else 5)
    lags, lag_vals = lag_corr(w, r_smooth, max_lag)
    valid = [(lag, val) for lag, val in zip(lags, lag_vals) if not np.isnan(val)]
    if valid:
        peak_lag, peak_r = max(valid, key=lambda t: abs(t[1]))
        peak_r2 = float(peak_r ** 2)
    else:
        peak_lag, peak_r2 = np.nan, np.nan
    # Build 3-panel figure: overlay, rolling r over time, cross-corr vs lag
    fig = plt.figure(figsize=(10, 10))
    gs = fig.add_gridspec(3, 1, height_ratios=[2, 1, 1], hspace=0.35)
    ax1 = fig.add_subplot(gs[0, 0])
    ax1.plot(steps, w, color='tab:blue', label='Avg Δw')
    ax1.set_ylabel('Avg Δw')
    ax1.set_title('Reward–Plasticity Coupling')
    ax1b = ax1.twinx()
    ax1b.bar(steps, r_smooth, color='tab:orange', alpha=0.25, label=f'Reward (win={window})')
    ax1b.set_ylabel('Reward (smoothed)')
    ax2 = fig.add_subplot(gs[1, 0], sharex=ax1)
    ax2.plot(steps, corr_series, color='tab:green', label=f'Rolling Pearson r (win={window})')
    ax2.axhline(0, color='#999', lw=0.8)
    ax2.set_ylabel('r')
    ax2.set_xlabel('Step')
    ax2.legend(loc='upper right')
    ax3 = fig.add_subplot(gs[2, 0])
    ax3.stem(lags, lag_vals, basefmt=" ", linefmt='tab:purple', markerfmt='tab:purple')
    ax3.axhline(0, color='#999', lw=0.8)
    title_suffix = f"peak lag={peak_lag}, r²={peak_r2:.3f}" if not np.isnan(peak_lag) else "no valid lag correlation"
    ax3.set_title(f'Cross-correlation vs Lag ({title_suffix})')
    ax3.set_xlabel('Lag (steps)')
    ax3.set_ylabel('Pearson r')
    save_plot(out_dir, 'fig5_reward_coupling.png')
    return {'peak_lag': peak_lag, 'peak_r2': peak_r2}


def plot_reward_coupling_legacy(learning_df: pd.DataFrame, reward_df: pd.DataFrame, out_dir: str):
    """Legacy 2-axis overlay (kept for backward compatibility)."""
    if reward_df is None or len(reward_df) == 0:
        return
    # Aggregate rewards per step
    rstep = reward_df.groupby('step', as_index=False)['reward'].count().rename(columns={'reward': 'reward_events'})
    merged = pd.merge(learning_df[['step', 'avg_weight_change']], rstep, on='step', how='left').fillna({'reward_events': 0})
    merged.sort_values('step', inplace=True)
    fig, ax1 = plt.subplots()
    ax1.plot(merged['step'], merged['avg_weight_change'], color='tab:blue', label='Avg Δw')
    ax1.set_xlabel('Step')
    ax1.set_ylabel('Avg Δw', color='tab:blue')
    ax1.tick_params(axis='y', labelcolor='tab:blue')
    ax2 = ax1.twinx()
    ax2.bar(merged['step'], merged['reward_events'], color='tab:orange', alpha=0.3, label='Reward events')
    ax2.set_ylabel('Reward events', color='tab:orange')
    ax2.tick_params(axis='y', labelcolor='tab:orange')
    plt.title('Reward–Plasticity Coupling (Legacy)')
    fig.legend(loc='upper right')
    save_plot(out_dir, 'reward_plasticity_coupling.png')


def plot_reward_scatter(learning_df: pd.DataFrame, reward_df: pd.DataFrame, out_dir: str, window: int = 10, max_lag: int = 20):
    if reward_df is None or len(reward_df) == 0:
        return
    merged = align_series(learning_df, reward_df)
    w = merged['avg_weight_change'].fillna(0).values
    r = merged['reward_events'].astype(float).values
    r_smooth = pd.Series(r).rolling(window, min_periods=1).mean().values if window and window > 1 else r

    # Compute lag spectrum and peak
    lags, lag_vals = lag_corr(w, r_smooth, max_lag)
    valid = [(lag, val) for lag, val in zip(lags, lag_vals) if not np.isnan(val)]
    if valid:
        peak_lag, peak_r = max(valid, key=lambda t: abs(t[1]))
    else:
        peak_lag, peak_r = 0, np.nan

    def align_at_lag(x: np.ndarray, y: np.ndarray, L: int):
        if L >= 0:
            xs = x[L:]
            ys = y[:len(xs)]
        else:
            ys = y[-L:]
            xs = x[:len(ys)]
        return xs, ys

    xs0, ys0 = align_at_lag(w, r_smooth, 0)
    xsP, ysP = align_at_lag(w, r_smooth, int(peak_lag))

    fig, axs = plt.subplots(1, 2, figsize=(12, 5))
    # Zero-lag scatter
    axs[0].scatter(xs0, ys0, alpha=0.6, s=12, color='tab:blue')
    if len(xs0) >= 2 and np.std(xs0) > 0 and np.std(ys0) > 0:
        r0 = np.corrcoef(xs0, ys0)[0, 1]
        axs[0].set_title(f'Zero-lag scatter (r²={r0**2:.3f})')
    else:
        axs[0].set_title('Zero-lag scatter')
    axs[0].set_xlabel('Avg Δw')
    axs[0].set_ylabel(f'Reward (win={window})')

    # Peak-lag scatter
    axs[1].scatter(xsP, ysP, alpha=0.6, s=12, color='tab:green')
    if len(xsP) >= 2 and np.std(xsP) > 0 and np.std(ysP) > 0 and not np.isnan(peak_r):
        axs[1].set_title(f'Peak-lag scatter (lag={peak_lag}, r²={peak_r**2:.3f})')
    else:
        axs[1].set_title(f'Peak-lag scatter (lag={peak_lag})')
    axs[1].set_xlabel('Avg Δw')
    axs[1].set_ylabel(f'Reward (win={window})')

    save_plot(out_dir, 'reward_coupling_scatter.png')


def plot_self_model(self_df: pd.DataFrame, out_dir: str):
    df = self_df.sort_values('step')
    plt.figure()
    plt.plot(df['step'], df['confidence'], label='Confidence', color='tab:purple')
    plt.xlabel('Step')
    plt.ylabel('Confidence')
    plt.title('Self-Model Confidence Trajectory')
    save_plot(out_dir, 'self_model_confidence.png')


def plot_hippocampal(hippo_df: pd.DataFrame, out_dir: str):
    df = hippo_df.sort_values('step')
    plt.figure()
    plt.plot(df['step'], df['significance'], label='Significance', color='tab:red')
    plt.plot(df['step'], df['priority'], label='Priority', color='tab:gray')
    plt.xlabel('Step')
    plt.ylabel('Score')
    plt.title('Hippocampal Snapshot Significance Timeline')
    plt.legend()
    save_plot(out_dir, 'hippocampal_snapshot_significance.png')


def main():
    ap = argparse.ArgumentParser(description='Generate figures for NeuroForge learning and reward analysis.')
    ap.add_argument('--learning-csv', required=True)
    ap.add_argument('--reward-csv', required=False, help='Optional reward_log CSV for coupling analysis')
    ap.add_argument('--self-csv', required=False)
    ap.add_argument('--hippo-csv', required=False)
    ap.add_argument('--out-dir', required=True)
    ap.add_argument('--window', type=int, default=10, help='Smoothing/rolling window used for reward correlation')
    ap.add_argument('--max-lag', type=int, default=20, help='Max lag (in steps) for cross-correlation')
    ap.add_argument('--reward-source', required=False, help='Filter reward CSV by source label')
    args = ap.parse_args()

    out_dir = os.path.abspath(args.out_dir)
    ensure_dir(out_dir)

    # Load data
    learning_df = pd.read_csv(args.learning_csv)
    reward_df = pd.read_csv(args.reward_csv) if args.reward_csv else None
    if reward_df is not None and args.reward_source and 'source' in reward_df.columns:
        reward_df = reward_df[reward_df['source'] == args.reward_source]
    self_df = pd.read_csv(args.self_csv) if args.self_csv else None
    hippo_df = pd.read_csv(args.hippo_csv) if args.hippo_csv else None

    # Required plots
    plot_learning_deltas(learning_df, out_dir, reward_df=reward_df, window=args.window)
    plot_ltp_ltd_ratio(learning_df, out_dir)
    # Extended reward–plasticity coupling (multi-panel)
    coupling_stats = None
    if reward_df is not None:
        coupling_stats = plot_reward_coupling_extended(learning_df, reward_df, out_dir, window=args.window, max_lag=args.max_lag)
        plot_reward_scatter(learning_df, reward_df, out_dir, window=args.window, max_lag=args.max_lag)
        # Legacy overlay retained for comparison
        plot_reward_coupling_legacy(learning_df, reward_df, out_dir)
    if self_df is not None and 'confidence' in self_df.columns:
        plot_self_model(self_df, out_dir)
    if hippo_df is not None and {'significance', 'priority'}.issubset(set(hippo_df.columns)):
        plot_hippocampal(hippo_df, out_dir)

    # Write simple summary
    summary_path = os.path.join(out_dir, 'summary.txt')
    with open(summary_path, 'w', encoding='utf-8') as f:
        f.write('NeuroForge Learning Analysis Summary\n')
        f.write('===================================\n')
        f.write(f"learning_csv: {os.path.abspath(args.learning_csv)}\n")
        if reward_df is not None:
            f.write(f"reward_csv: {os.path.abspath(args.reward_csv)}\n")
            if args.reward_source:
                f.write(f"reward_source: {args.reward_source}\n")
        if self_df is not None:
            f.write(f"self_csv: {os.path.abspath(args.self_csv)}\n")
        if hippo_df is not None:
            f.write(f"hippo_csv: {os.path.abspath(args.hippo_csv)}\n")
        # Basic stats
        f.write(f"\nSteps observed (learning): {learning_df['step'].max()}\n")
        if reward_df is not None:
            f.write(f"Total reward events: {len(reward_df)}\n")
        else:
            f.write("Total reward events: N/A (no reward CSV)\n")
        # Figures
        figs = ['learning_delta_w_vs_time', 'ltp_ltd_ratio']
        if reward_df is not None:
            figs += ['reward_plasticity_coupling', 'fig5_reward_coupling', 'reward_coupling_scatter']
        if self_df is not None:
            figs += ['self_model_confidence']
        if hippo_df is not None:
            figs += ['hippocampal_snapshot_significance']
        f.write("Figures generated: " + ", ".join(figs) + "\n")
        # Coupling stats
        if coupling_stats is not None and coupling_stats.get('peak_lag') is not None:
            peak_lag = coupling_stats['peak_lag']
            peak_r2 = coupling_stats['peak_r2']
            f.write(f"Lag peak (steps): {peak_lag}\n")
            f.write(f"Peak r²: {peak_r2:.4f}\n")
    print(f"Wrote summary: {summary_path}")


if __name__ == '__main__':
    main()