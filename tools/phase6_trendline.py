#!/usr/bin/env python3
"""
Generate Phase 6 longitudinal trend plots from historical sweep summaries.

Scans pages/tags/runner/ for phase6_sweep_summary_*.csv files (timestamped from CI runs),
plots margin vs ΔReward trends over time, and outputs:
 - pages/tags/runner/phase6_trend.png
 - Updates pages/tags_meta.json with trend metadata

Usage:
    python tools/phase6_trendline.py [--input-dir pages/tags/runner] [--output pages/tags/runner/phase6_trend.png]
"""
import argparse
import csv
import glob
import json
import os
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Any

import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.patches import Rectangle


def parse_args():
    ap = argparse.ArgumentParser(description='Generate Phase 6 longitudinal trend plots')
    ap.add_argument('--input-dir', default='pages/tags/runner', 
                    help='Directory containing phase6_sweep_summary*.csv files')
    ap.add_argument('--output', default='pages/tags/runner/phase6_trend.png',
                    help='Output PNG path for trend plot')
    ap.add_argument('--meta-json', default='pages/tags_meta.json',
                    help='Path to tags_meta.json to update with trend info')
    return ap.parse_args()


def find_sweep_csvs(input_dir: str) -> List[str]:
    """Find all phase6_sweep_summary*.csv files in the input directory."""
    pattern = os.path.join(input_dir, 'phase6_sweep_summary*.csv')
    files = glob.glob(pattern)
    # Also include the main summary file without timestamp
    main_file = os.path.join(input_dir, 'phase6_sweep_summary.csv')
    if os.path.exists(main_file):
        files.append(main_file)
    return sorted(set(files))


def extract_date_from_filename(filepath: str) -> datetime:
    """Extract date from filename or use file modification time."""
    filename = os.path.basename(filepath)
    
    # Try to parse date from filename like phase6_sweep_summary_2024-01-15.csv
    if '_' in filename:
        parts = filename.replace('.csv', '').split('_')
        for part in parts:
            if '-' in part and len(part) >= 8:  # YYYY-MM-DD format
                try:
                    return datetime.strptime(part, '%Y-%m-%d')
                except ValueError:
                    continue
    
    # Fallback to file modification time
    mtime = os.path.getmtime(filepath)
    return datetime.fromtimestamp(mtime)


def load_sweep_data(csv_files: List[str]) -> Dict[str, List[Dict[str, Any]]]:
    """Load sweep data from CSV files, organized by date."""
    data_by_date = {}
    
    for csv_file in csv_files:
        try:
            date = extract_date_from_filename(csv_file)
            date_str = date.strftime('%Y-%m-%d')
            
            with open(csv_file, 'r', newline='', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                rows = list(reader)
                
            # Add date info to each row
            for row in rows:
                row['date'] = date
                row['date_str'] = date_str
                
            data_by_date[date_str] = rows
            
        except Exception as e:
            print(f"Warning: Could not process {csv_file}: {e}")
            continue
    
    return data_by_date


def create_trend_plot(data_by_date: Dict[str, List[Dict[str, Any]]], output_path: str):
    """Create and save the trend plot."""
    if not data_by_date:
        print("No data found for trend plotting")
        return
    
    # Prepare data for plotting
    dates = []
    margins = []
    delta_rewards = []
    override_rates = []
    
    # Sort by date
    sorted_dates = sorted(data_by_date.keys())
    
    for date_str in sorted_dates:
        rows = data_by_date[date_str]
        date_obj = datetime.strptime(date_str, '%Y-%m-%d')
        
        for row in rows:
            try:
                margin = float(row.get('margin', 0))
                delta_reward = float(row.get('delta_reward_vs_baseline', 0))
                override_rate = float(row.get('override_rate', 0))
                
                dates.append(date_obj)
                margins.append(margin)
                delta_rewards.append(delta_reward)
                override_rates.append(override_rate)
                
            except (ValueError, TypeError):
                continue
    
    if not dates:
        print("No valid data points found for plotting")
        return
    
    # Create the plot
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
    fig.suptitle('Phase 6 Longitudinal Trends', fontsize=16, fontweight='bold')
    
    # Plot 1: Margin vs ΔReward over time
    unique_margins = sorted(set(margins))
    colors = plt.cm.viridis(np.linspace(0, 1, len(unique_margins)))
    
    for i, margin in enumerate(unique_margins):
        margin_dates = [d for d, m in zip(dates, margins) if abs(m - margin) < 0.001]
        margin_deltas = [dr for dr, m in zip(delta_rewards, margins) if abs(m - margin) < 0.001]
        
        if margin_dates:
            ax1.plot(margin_dates, margin_deltas, 'o-', 
                    color=colors[i], label=f'Margin {margin:.2f}', 
                    linewidth=2, markersize=6)
    
    ax1.set_xlabel('Date')
    ax1.set_ylabel('ΔReward vs Baseline')
    ax1.set_title('Margin Performance Trends')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    ax1.xaxis.set_major_formatter(mdates.DateFormatter('%m/%d'))
    ax1.xaxis.set_major_locator(mdates.DayLocator(interval=1))
    
    # Plot 2: Override Rate trends
    for i, margin in enumerate(unique_margins):
        margin_dates = [d for d, m in zip(dates, margins) if abs(m - margin) < 0.001]
        margin_overrides = [ovr for ovr, m in zip(override_rates, margins) if abs(m - margin) < 0.001]
        
        if margin_dates:
            ax2.plot(margin_dates, margin_overrides, 's-', 
                    color=colors[i], label=f'Margin {margin:.2f}', 
                    linewidth=2, markersize=6)
    
    ax2.set_xlabel('Date')
    ax2.set_ylabel('Override Rate')
    ax2.set_title('Override Rate Trends')
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    ax2.xaxis.set_major_formatter(mdates.DateFormatter('%m/%d'))
    ax2.xaxis.set_major_locator(mdates.DayLocator(interval=1))
    
    # Rotate x-axis labels for better readability
    plt.setp(ax1.xaxis.get_majorticklabels(), rotation=45)
    plt.setp(ax2.xaxis.get_majorticklabels(), rotation=45)
    
    plt.tight_layout()
    
    # Ensure output directory exists
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    # Save the plot
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    
    print(f"Trend plot saved to: {output_path}")


def update_meta_json(meta_path: str, trend_info: Dict[str, Any]):
    """Update tags_meta.json with trend information."""
    meta = {}
    
    if os.path.exists(meta_path):
        try:
            with open(meta_path, 'r', encoding='utf-8') as f:
                meta = json.load(f)
        except (json.JSONDecodeError, FileNotFoundError):
            meta = {}
    
    if not isinstance(meta, dict):
        meta = {}
    
    meta['phase6_trend'] = trend_info
    
    # Ensure directory exists
    os.makedirs(os.path.dirname(meta_path), exist_ok=True)
    
    with open(meta_path, 'w', encoding='utf-8') as f:
        json.dump(meta, f, indent=2)
    
    print(f"Updated meta JSON: {meta_path}")


def main():
    args = parse_args()
    
    # Find CSV files
    csv_files = find_sweep_csvs(args.input_dir)
    if not csv_files:
        print(f"No phase6_sweep_summary*.csv files found in {args.input_dir}")
        return
    
    print(f"Found {len(csv_files)} sweep summary files")
    
    # Load data
    data_by_date = load_sweep_data(csv_files)
    if not data_by_date:
        print("No valid data loaded from CSV files")
        return
    
    print(f"Loaded data for {len(data_by_date)} dates: {sorted(data_by_date.keys())}")
    
    # Create trend plot
    create_trend_plot(data_by_date, args.output)
    
    # Update meta JSON
    trend_info = {
        'display': 'Phase 6 Longitudinal Trends',
        'image': os.path.basename(args.output),
        'updated': datetime.now().isoformat(),
        'description': 'Weekly trend analysis showing margin performance and override rates over time.',
        'data_points': sum(len(rows) for rows in data_by_date.values()),
        'date_range': {
            'start': min(data_by_date.keys()),
            'end': max(data_by_date.keys())
        }
    }
    
    update_meta_json(args.meta_json, trend_info)


if __name__ == '__main__':
    # Import numpy here to avoid import errors if matplotlib is not available
    try:
        import numpy as np
    except ImportError:
        print("Error: numpy is required for trend plotting")
        exit(1)
    
    main()