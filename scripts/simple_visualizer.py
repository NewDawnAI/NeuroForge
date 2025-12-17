#!/usr/bin/env python3
"""
Simple NeuroForge Developmental Visualizer

A lightweight visualization tool for NeuroForge developmental tracking data.
"""

import csv
import os
from pathlib import Path
import argparse

def load_trajectory_data(log_dir):
    """Load trajectory data from CSV files."""
    trajectory_file = Path(log_dir) / "token_trajectories.csv"
    cluster_file = Path(log_dir) / "cluster_evolution.csv"
    
    trajectory_data = []
    cluster_data = []
    
    # Load trajectory data
    if trajectory_file.exists():
        try:
            with open(trajectory_file, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                trajectory_data = list(reader)
        except Exception as e:
            print(f"⚠️ Error loading trajectory data: {e}")
    
    # Load cluster data
    if cluster_file.exists():
        try:
            with open(cluster_file, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                cluster_data = list(reader)
        except Exception as e:
            print(f"⚠️ Error loading cluster data: {e}")
    
    return trajectory_data, cluster_data

def generate_text_report(trajectory_data, cluster_data, output_file):
    """Generate a comprehensive text-based developmental report."""
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write("🧠 NeuroForge Developmental Analysis Report\n")
        f.write("=" * 50 + "\n\n")
        
        # Token trajectory analysis
        if trajectory_data:
            f.write("📈 Token Development Trajectories\n")
            f.write("-" * 35 + "\n\n")
            
            # Group by token
            tokens = {}
            for row in trajectory_data:
                symbol = row['symbol']
                if symbol not in tokens:
                    tokens[symbol] = []
                tokens[symbol].append({
                    'timestamp': int(row['timestamp']),
                    'activation': float(row['activation_strength']),
                    'usage': int(row['usage_count']),
                    'stability': float(row['cluster_stability']),
                    'cross_modal': float(row['cross_modal_strength']),
                    'stage': int(row['stage']),
                    'associated': row['associated_tokens'].split(';') if row['associated_tokens'] else []
                })
            
            # Analyze each token's development
            for symbol, history in tokens.items():
                history.sort(key=lambda x: x['timestamp'])
                
                f.write(f"🔤 Token: '{symbol}'\n")
                f.write(f"   Development Steps: {len(history)}\n")
                
                if len(history) >= 2:
                    initial = history[0]
                    final = history[-1]
                    
                    activation_growth = final['activation'] - initial['activation']
                    usage_growth = final['usage'] - initial['usage']
                    stability_growth = final['stability'] - initial['stability']
                    
                    f.write(f"   Activation Growth: {initial['activation']:.3f} → {final['activation']:.3f} (+{activation_growth:.3f})\n")
                    f.write(f"   Usage Growth: {initial['usage']} → {final['usage']} (+{usage_growth})\n")
                    f.write(f"   Stability Growth: {initial['stability']:.3f} → {final['stability']:.3f} (+{stability_growth:.3f})\n")
                    f.write(f"   Cross-Modal Strength: {final['cross_modal']:.3f}\n")
                    
                    if final['associated']:
                        f.write(f"   Associated Tokens: {', '.join(final['associated'])}\n")
                    
                    # Developmental assessment
                    if activation_growth > 0.3:
                        f.write("   ✅ Strong developmental progress detected\n")
                    elif activation_growth > 0.1:
                        f.write("   📈 Moderate developmental progress\n")
                    else:
                        f.write("   📊 Early development stage\n")
                
                f.write("\n")
        
        # Cluster analysis
        if cluster_data:
            f.write("🔗 Cluster Formation Analysis\n")
            f.write("-" * 30 + "\n\n")
            
            proto_words = []
            regular_clusters = []
            
            for row in cluster_data:
                cluster_info = {
                    'name': row['cluster_name'],
                    'formation_step': int(row['formation_step']),
                    'member_count': int(row['member_count']),
                    'cohesion': float(row['cohesion_score']),
                    'is_proto_word': row['is_proto_word'].lower() == 'true',
                    'members': row['members'].split(';') if row['members'] else []
                }
                
                if cluster_info['is_proto_word']:
                    proto_words.append(cluster_info)
                else:
                    regular_clusters.append(cluster_info)
            
            # Proto-word analysis
            if proto_words:
                f.write("🎯 Proto-Word Formations:\n")
                for proto in sorted(proto_words, key=lambda x: x['formation_step']):
                    f.write(f"   • {proto['name']} (Step {proto['formation_step']})\n")
                    f.write(f"     Members: {', '.join(proto['members'])}\n")
                    f.write(f"     Cohesion: {proto['cohesion']:.3f}\n")
                    f.write(f"     Size: {proto['member_count']} tokens\n")
                    
                    # Assess proto-word quality
                    if proto['cohesion'] > 0.7:
                        f.write("     ✅ High-quality proto-word formation\n")
                    elif proto['cohesion'] > 0.5:
                        f.write("     📈 Developing proto-word structure\n")
                    else:
                        f.write("     📊 Early clustering detected\n")
                    f.write("\n")
            
            # Regular clusters
            if regular_clusters:
                f.write("📊 Regular Token Clusters:\n")
                for cluster in sorted(regular_clusters, key=lambda x: x['formation_step']):
                    f.write(f"   • {cluster['name']} (Step {cluster['formation_step']})\n")
                    f.write(f"     Members: {', '.join(cluster['members'])}\n")
                    f.write(f"     Cohesion: {cluster['cohesion']:.3f}\n")
                    f.write("\n")
        
        # Developmental milestones
        f.write("🏆 Developmental Milestones Assessment\n")
        f.write("-" * 40 + "\n\n")
        
        milestones_achieved = []
        
        if trajectory_data:
            # Check vocabulary size
            unique_tokens = len(set(row['symbol'] for row in trajectory_data))
            if unique_tokens >= 5:
                milestones_achieved.append(f"✅ Vocabulary Formation: {unique_tokens} unique tokens")
            
            # Check activation levels
            max_activation = max(float(row['activation_strength']) for row in trajectory_data)
            if max_activation > 0.5:
                milestones_achieved.append(f"✅ Strong Token Activation: {max_activation:.3f} peak")
            
            # Check cross-modal integration
            max_cross_modal = max(float(row['cross_modal_strength']) for row in trajectory_data)
            if max_cross_modal > 0.3:
                milestones_achieved.append(f"✅ Cross-Modal Integration: {max_cross_modal:.3f} strength")
        
        if cluster_data:
            proto_word_count = sum(1 for row in cluster_data if row['is_proto_word'].lower() == 'true')
            if proto_word_count > 0:
                milestones_achieved.append(f"✅ Proto-Word Formation: {proto_word_count} detected")
        
        if milestones_achieved:
            for milestone in milestones_achieved:
                f.write(f"{milestone}\n")
        else:
            f.write("📊 Early developmental stage - milestones pending\n")
        
        f.write("\n" + "=" * 50 + "\n")
        f.write("Report generated successfully! 🎉\n")

def generate_simple_charts(trajectory_data, cluster_data, output_dir):
    """Generate simple ASCII-based charts."""
    
    charts_file = Path(output_dir) / "developmental_charts.txt"
    
    with open(charts_file, 'w', encoding='utf-8') as f:
        f.write("📊 NeuroForge Developmental Charts\n")
        f.write("=" * 40 + "\n\n")
        
        if trajectory_data:
            # Token activation chart
            f.write("📈 Token Activation Over Time\n")
            f.write("-" * 30 + "\n")
            
            # Group by token and create simple bar charts
            tokens = {}
            for row in trajectory_data:
                symbol = row['symbol']
                if symbol not in tokens:
                    tokens[symbol] = []
                tokens[symbol].append(float(row['activation_strength']))
            
            for symbol, activations in tokens.items():
                f.write(f"\n{symbol}: ")
                for activation in activations:
                    bar_length = int(activation * 20)  # Scale to 20 chars max
                    f.write("█" * bar_length + f" {activation:.3f}")
                    f.write("\n" + " " * (len(symbol) + 2))
                f.write("\n")
        
        if cluster_data:
            f.write("\n🔗 Cluster Formation Timeline\n")
            f.write("-" * 30 + "\n")
            
            clusters = sorted(cluster_data, key=lambda x: int(x['formation_step']))
            
            for cluster in clusters:
                step = int(cluster['formation_step'])
                name = cluster['cluster_name']
                cohesion = float(cluster['cohesion_score'])
                is_proto = cluster['is_proto_word'].lower() == 'true'
                
                marker = "🎯" if is_proto else "📊"
                bar_length = int(cohesion * 15)
                
                f.write(f"Step {step:3d}: {marker} {name}\n")
                f.write(f"          Cohesion: {'█' * bar_length} {cohesion:.3f}\n")
                f.write(f"          Members: {cluster['members']}\n\n")

def main():
    parser = argparse.ArgumentParser(description='Simple NeuroForge Development Visualizer')
    parser.add_argument('--log-dir', default='trajectory_logs', help='Directory containing trajectory logs')
    parser.add_argument('--mode', choices=['report', 'charts', 'both'], default='both',
                       help='Type of visualization to generate')
    
    args = parser.parse_args()
    
    print("🎨 Simple NeuroForge Development Visualizer")
    print(f"📂 Loading data from: {args.log_dir}")
    
    # Load data
    trajectory_data, cluster_data = load_trajectory_data(args.log_dir)
    
    if not trajectory_data and not cluster_data:
        print("❌ No data found. Please run the developmental tracking test first.")
        return
    
    print(f"📊 Loaded {len(trajectory_data)} trajectory records and {len(cluster_data)} cluster records")
    
    # Generate outputs
    output_dir = Path(args.log_dir)
    
    if args.mode in ['report', 'both']:
        report_file = output_dir / "developmental_analysis.txt"
        generate_text_report(trajectory_data, cluster_data, report_file)
        print(f"📋 Generated analysis report: {report_file}")
    
    if args.mode in ['charts', 'both']:
        generate_simple_charts(trajectory_data, cluster_data, output_dir)
        charts_file = output_dir / "developmental_charts.txt"
        print(f"📊 Generated charts: {charts_file}")
    
    print("✅ Visualization complete!")

if __name__ == "__main__":
    main()