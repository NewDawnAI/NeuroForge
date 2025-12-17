#!/usr/bin/env python3
"""
NeuroForge Developmental Trajectory Visualizer

This script creates real-time visualizations of token association trajectories,
cluster evolution, and developmental milestones from NeuroForge's language learning system.
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
import seaborn as sns
from pathlib import Path
import argparse
import json
from datetime import datetime
import warnings
warnings.filterwarnings('ignore')

# Set style for publication-quality plots
plt.style.use('seaborn-v0_8-darkgrid')
sns.set_palette("husl")

class NeuroForgeDevelopmentVisualizer:
    def __init__(self, log_directory="trajectory_logs"):
        self.log_dir = Path(log_directory)
        self.trajectory_file = self.log_dir / "token_trajectories.csv"
        self.cluster_file = self.log_dir / "cluster_evolution.csv"
        
        # Visualization settings
        self.fig_size = (16, 12)
        self.update_interval = 1000  # ms
        
        # Data containers
        self.trajectory_data = None
        self.cluster_data = None
        self.last_update = None
        
        print(f"🎨 NeuroForge Development Visualizer initialized")
        print(f"📂 Monitoring: {self.log_dir}")
    
    def load_data(self):
        """Load trajectory and cluster data from CSV files."""
        try:
            if self.trajectory_file.exists():
                self.trajectory_data = pd.read_csv(self.trajectory_file)
                self.trajectory_data['timestamp'] = pd.to_datetime(self.trajectory_data['timestamp'], unit='ms')
                
            if self.cluster_file.exists():
                self.cluster_data = pd.read_csv(self.cluster_file)
                
            self.last_update = datetime.now()
            return True
            
        except Exception as e:
            print(f"⚠️ Error loading data: {e}")
            return False
    
    def create_static_dashboard(self):
        """Create a comprehensive static dashboard of developmental progress."""
        if not self.load_data():
            print("❌ No data available for visualization")
            return
        
        fig, axes = plt.subplots(2, 3, figsize=(18, 12))
        fig.suptitle('NeuroForge Developmental Language Learning Dashboard', fontsize=16, fontweight='bold')
        
        # 1. Token Activation Trajectories
        self.plot_token_trajectories(axes[0, 0])
        
        # 2. Cluster Evolution Timeline
        self.plot_cluster_evolution(axes[0, 1])
        
        # 3. Cross-Modal Binding Strength
        self.plot_cross_modal_binding(axes[0, 2])
        
        # 4. Developmental Stage Progression
        self.plot_stage_progression(axes[1, 0])
        
        # 5. Vocabulary Growth
        self.plot_vocabulary_growth(axes[1, 1])
        
        # 6. Token Cluster Stability
        self.plot_cluster_stability(axes[1, 2])
        
        plt.tight_layout()
        
        # Save dashboard
        output_file = self.log_dir / f"developmental_dashboard_{datetime.now().strftime('%Y%m%d_%H%M%S')}.png"
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"📊 Dashboard saved: {output_file}")
        
        plt.show()
    
    def plot_token_trajectories(self, ax):
        """Plot token activation strength trajectories over time."""
        if self.trajectory_data is None or self.trajectory_data.empty:
            ax.text(0.5, 0.5, 'No trajectory data', ha='center', va='center', transform=ax.transAxes)
            ax.set_title('Token Activation Trajectories')
            return
        
        # Get top tokens by final activation
        final_activations = self.trajectory_data.groupby('symbol')['activation_strength'].last().sort_values(ascending=False)
        top_tokens = final_activations.head(8).index
        
        for token in top_tokens:
            token_data = self.trajectory_data[self.trajectory_data['symbol'] == token]
            if not token_data.empty:
                ax.plot(token_data['timestamp'], token_data['activation_strength'], 
                       marker='o', markersize=3, label=token, linewidth=2, alpha=0.8)
        
        ax.set_xlabel('Time')
        ax.set_ylabel('Activation Strength')
        ax.set_title('Token Activation Trajectories')
        ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
        ax.grid(True, alpha=0.3)
    
    def plot_cluster_evolution(self, ax):
        """Plot cluster formation and evolution over time."""
        if self.cluster_data is None or self.cluster_data.empty:
            ax.text(0.5, 0.5, 'No cluster data', ha='center', va='center', transform=ax.transAxes)
            ax.set_title('Cluster Evolution Timeline')
            return
        
        # Plot cluster formation over time
        cluster_counts = self.cluster_data.groupby('formation_step').size().cumsum()
        
        ax.plot(cluster_counts.index, cluster_counts.values, 
               marker='s', markersize=6, linewidth=3, color='darkblue', alpha=0.8)
        
        # Highlight proto-word formations
        proto_words = self.cluster_data[self.cluster_data['is_proto_word'] == True]
        if not proto_words.empty:
            ax.scatter(proto_words['formation_step'], 
                      [cluster_counts.loc[step] if step in cluster_counts.index else 0 for step in proto_words['formation_step']], 
                      color='red', s=100, marker='*', label='Proto-words', zorder=5)
        
        ax.set_xlabel('Development Step')
        ax.set_ylabel('Cumulative Clusters')
        ax.set_title('Cluster Evolution Timeline')
        ax.legend()
        ax.grid(True, alpha=0.3)
    
    def plot_cross_modal_binding(self, ax):
        """Plot cross-modal binding strength distribution."""
        if self.trajectory_data is None or self.trajectory_data.empty:
            ax.text(0.5, 0.5, 'No trajectory data', ha='center', va='center', transform=ax.transAxes)
            ax.set_title('Cross-Modal Binding Strength')
            return
        
        # Get latest cross-modal strengths
        latest_data = self.trajectory_data.groupby('symbol').last()
        
        if 'cross_modal_strength' in latest_data.columns:
            ax.hist(latest_data['cross_modal_strength'], bins=20, alpha=0.7, color='green', edgecolor='black')
            ax.axvline(latest_data['cross_modal_strength'].mean(), color='red', linestyle='--', 
                      label=f'Mean: {latest_data["cross_modal_strength"].mean():.3f}')
        
        ax.set_xlabel('Cross-Modal Binding Strength')
        ax.set_ylabel('Number of Tokens')
        ax.set_title('Cross-Modal Binding Distribution')
        ax.legend()
        ax.grid(True, alpha=0.3)
    
    def plot_stage_progression(self, ax):
        """Plot developmental stage progression over time."""
        if self.trajectory_data is None or self.trajectory_data.empty:
            ax.text(0.5, 0.5, 'No trajectory data', ha='center', va='center', transform=ax.transAxes)
            ax.set_title('Developmental Stage Progression')
            return
        
        # Map stages to numbers for plotting
        stage_mapping = {0: 'Chaos', 1: 'Babbling', 2: 'Mimicry', 3: 'Grounding', 4: 'Reflection', 5: 'Communication'}
        
        # Get stage progression over time
        stage_progression = self.trajectory_data.groupby('timestamp')['stage_at_snapshot'].first()
        
        ax.step(stage_progression.index, stage_progression.values, where='post', linewidth=3, color='purple')
        ax.fill_between(stage_progression.index, stage_progression.values, alpha=0.3, color='purple', step='post')
        
        ax.set_xlabel('Time')
        ax.set_ylabel('Developmental Stage')
        ax.set_title('Developmental Stage Progression')
        ax.set_yticks(list(stage_mapping.keys()))
        ax.set_yticklabels(list(stage_mapping.values()), rotation=45)
        ax.grid(True, alpha=0.3)
    
    def plot_vocabulary_growth(self, ax):
        """Plot vocabulary size growth over time."""
        if self.trajectory_data is None or self.trajectory_data.empty:
            ax.text(0.5, 0.5, 'No trajectory data', ha='center', va='center', transform=ax.transAxes)
            ax.set_title('Vocabulary Growth')
            return
        
        # Calculate vocabulary size over time
        vocab_growth = self.trajectory_data.groupby('timestamp')['symbol'].nunique().cumsum()
        
        ax.plot(vocab_growth.index, vocab_growth.values, marker='o', markersize=4, 
               linewidth=3, color='orange', alpha=0.8)
        
        # Add growth rate annotation
        if len(vocab_growth) > 1:
            growth_rate = (vocab_growth.iloc[-1] - vocab_growth.iloc[0]) / len(vocab_growth)
            ax.text(0.05, 0.95, f'Growth Rate: {growth_rate:.2f} tokens/step', 
                   transform=ax.transAxes, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
        
        ax.set_xlabel('Time')
        ax.set_ylabel('Vocabulary Size')
        ax.set_title('Vocabulary Growth Over Time')
        ax.grid(True, alpha=0.3)
    
    def plot_cluster_stability(self, ax):
        """Plot cluster stability metrics."""
        if self.cluster_data is None or self.cluster_data.empty:
            ax.text(0.5, 0.5, 'No cluster data', ha='center', va='center', transform=ax.transAxes)
            ax.set_title('Cluster Stability Analysis')
            return
        
        # Plot cohesion scores
        ax.scatter(self.cluster_data['formation_step'], self.cluster_data['cohesion_score'], 
                  alpha=0.7, s=60, c=self.cluster_data['member_count'], cmap='viridis')
        
        # Add colorbar
        cbar = plt.colorbar(ax.collections[0], ax=ax)
        cbar.set_label('Cluster Size')
        
        # Highlight proto-words
        proto_words = self.cluster_data[self.cluster_data['is_proto_word'] == True]
        if not proto_words.empty:
            ax.scatter(proto_words['formation_step'], proto_words['cohesion_score'], 
                      color='red', s=100, marker='*', label='Proto-words', zorder=5)
            ax.legend()
        
        ax.set_xlabel('Formation Step')
        ax.set_ylabel('Cohesion Score')
        ax.set_title('Cluster Stability Analysis')
        ax.grid(True, alpha=0.3)
    
    def create_animated_visualization(self):
        """Create an animated visualization of developmental progress."""
        if not self.load_data():
            print("❌ No data available for animation")
            return
        
        fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('NeuroForge Live Developmental Tracking', fontsize=14, fontweight='bold')
        
        def animate(frame):
            # Clear axes
            for ax in [ax1, ax2, ax3, ax4]:
                ax.clear()
            
            # Reload data
            self.load_data()
            
            if self.trajectory_data is not None and not self.trajectory_data.empty:
                # Limit data to current frame
                current_data = self.trajectory_data.iloc[:frame*10] if frame*10 < len(self.trajectory_data) else self.trajectory_data
                
                # Plot 1: Token trajectories
                top_tokens = current_data.groupby('symbol')['activation_strength'].last().nlargest(5).index
                for token in top_tokens:
                    token_data = current_data[current_data['symbol'] == token]
                    ax1.plot(token_data.index, token_data['activation_strength'], label=token, marker='o', markersize=2)
                ax1.set_title('Token Activation Trajectories')
                ax1.set_xlabel('Time Step')
                ax1.set_ylabel('Activation')
                ax1.legend(fontsize=8)
                ax1.grid(True, alpha=0.3)
                
                # Plot 2: Vocabulary growth
                vocab_growth = current_data.groupby(current_data.index)['symbol'].nunique().cumsum()
                ax2.plot(vocab_growth.index, vocab_growth.values, color='orange', linewidth=2)
                ax2.set_title('Vocabulary Growth')
                ax2.set_xlabel('Time Step')
                ax2.set_ylabel('Vocabulary Size')
                ax2.grid(True, alpha=0.3)
                
                # Plot 3: Stage progression
                if 'stage_at_snapshot' in current_data.columns:
                    stage_data = current_data.groupby(current_data.index)['stage_at_snapshot'].first()
                    ax3.step(stage_data.index, stage_data.values, where='post', color='purple', linewidth=2)
                    ax3.set_title('Developmental Stage')
                    ax3.set_xlabel('Time Step')
                    ax3.set_ylabel('Stage')
                    ax3.grid(True, alpha=0.3)
                
                # Plot 4: Cluster formation
                if self.cluster_data is not None and not self.cluster_data.empty:
                    current_clusters = self.cluster_data[self.cluster_data['formation_step'] <= frame*10]
                    cluster_counts = current_clusters.groupby('formation_step').size().cumsum()
                    ax4.plot(cluster_counts.index, cluster_counts.values, color='darkblue', linewidth=2, marker='s')
                    ax4.set_title('Cluster Formation')
                    ax4.set_xlabel('Formation Step')
                    ax4.set_ylabel('Cumulative Clusters')
                    ax4.grid(True, alpha=0.3)
            
            plt.tight_layout()
        
        # Create animation
        anim = animation.FuncAnimation(fig, animate, interval=self.update_interval, cache_frame_data=False)
        
        # Save animation
        output_file = self.log_dir / f"developmental_animation_{datetime.now().strftime('%Y%m%d_%H%M%S')}.gif"
        try:
            anim.save(output_file, writer='pillow', fps=2)
            print(f"🎬 Animation saved: {output_file}")
        except Exception as e:
            print(f"⚠️ Could not save animation: {e}")
        
        plt.show()
    
    def generate_milestone_report(self):
        """Generate a text-based milestone achievement report."""
        if not self.load_data():
            print("❌ No data available for milestone report")
            return
        
        report_file = self.log_dir / f"milestone_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        
        with open(report_file, 'w') as f:
            f.write("🧠 NeuroForge Developmental Milestone Report\n")
            f.write("=" * 50 + "\n\n")
            f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
            
            if self.trajectory_data is not None and not self.trajectory_data.empty:
                # Basic statistics
                total_tokens = self.trajectory_data['symbol'].nunique()
                max_activation = self.trajectory_data['activation_strength'].max()
                avg_activation = self.trajectory_data['activation_strength'].mean()
                
                f.write(f"📊 Basic Statistics:\n")
                f.write(f"   Total Unique Tokens: {total_tokens}\n")
                f.write(f"   Maximum Activation: {max_activation:.3f}\n")
                f.write(f"   Average Activation: {avg_activation:.3f}\n\n")
                
                # Top performing tokens
                top_tokens = self.trajectory_data.groupby('symbol')['activation_strength'].max().nlargest(10)
                f.write(f"🏆 Top Performing Tokens:\n")
                for i, (token, activation) in enumerate(top_tokens.items(), 1):
                    f.write(f"   {i:2d}. {token:10s} (activation: {activation:.3f})\n")
                f.write("\n")
                
                # Developmental stages reached
                if 'stage_at_snapshot' in self.trajectory_data.columns:
                    max_stage = self.trajectory_data['stage_at_snapshot'].max()
                    stage_names = ['Chaos', 'Babbling', 'Mimicry', 'Grounding', 'Reflection', 'Communication']
                    f.write(f"🎯 Developmental Progress:\n")
                    f.write(f"   Highest Stage Reached: {stage_names[min(max_stage, len(stage_names)-1)]}\n")
                    f.write(f"   Stage Index: {max_stage}/5\n\n")
            
            if self.cluster_data is not None and not self.cluster_data.empty:
                # Cluster analysis
                total_clusters = len(self.cluster_data)
                proto_words = self.cluster_data[self.cluster_data['is_proto_word'] == True]
                proto_word_count = len(proto_words)
                
                f.write(f"🔗 Cluster Analysis:\n")
                f.write(f"   Total Clusters Formed: {total_clusters}\n")
                f.write(f"   Proto-words Detected: {proto_word_count}\n")
                
                if proto_word_count > 0:
                    f.write(f"   Proto-word Examples:\n")
                    for _, proto in proto_words.head(5).iterrows():
                        f.write(f"      - {proto['cluster_name']} (members: {proto['members']})\n")
                f.write("\n")
                
                # Cluster quality
                avg_cohesion = self.cluster_data['cohesion_score'].mean()
                max_cohesion = self.cluster_data['cohesion_score'].max()
                f.write(f"   Average Cluster Cohesion: {avg_cohesion:.3f}\n")
                f.write(f"   Maximum Cluster Cohesion: {max_cohesion:.3f}\n\n")
            
            # Milestone achievements
            f.write("🏅 Milestone Achievements:\n")
            
            milestones = []
            if self.trajectory_data is not None and not self.trajectory_data.empty:
                if self.trajectory_data['symbol'].nunique() >= 5:
                    milestones.append("✅ First Vocabulary (5+ tokens)")
                if self.trajectory_data['activation_strength'].max() > 0.5:
                    milestones.append("✅ Strong Token Activation (>0.5)")
                if 'usage_count' in self.trajectory_data.columns and self.trajectory_data['usage_count'].max() > 3:
                    milestones.append("✅ Token Reinforcement (3+ uses)")
            
            if self.cluster_data is not None and not self.cluster_data.empty:
                if len(self.cluster_data) >= 3:
                    milestones.append("✅ Cluster Formation (3+ clusters)")
                if (self.cluster_data['is_proto_word'] == True).any():
                    milestones.append("✅ Proto-word Detection")
                if self.cluster_data['cohesion_score'].max() > 0.3:
                    milestones.append("✅ Stable Clustering (cohesion >0.3)")
            
            if milestones:
                for milestone in milestones:
                    f.write(f"   {milestone}\n")
            else:
                f.write("   No major milestones achieved yet (early development stage)\n")
            
            f.write("\n" + "=" * 50 + "\n")
            f.write("End of Report\n")
        
        print(f"📋 Milestone report saved: {report_file}")

def main():
    parser = argparse.ArgumentParser(description='NeuroForge Developmental Trajectory Visualizer')
    parser.add_argument('--log-dir', default='trajectory_logs', help='Directory containing trajectory logs')
    parser.add_argument('--mode', choices=['static', 'animated', 'report'], default='static',
                       help='Visualization mode: static dashboard, animated view, or milestone report')
    parser.add_argument('--output', help='Output file path (optional)')
    
    args = parser.parse_args()
    
    visualizer = NeuroForgeDevelopmentVisualizer(args.log_dir)
    
    if args.mode == 'static':
        print("🎨 Creating static developmental dashboard...")
        visualizer.create_static_dashboard()
    elif args.mode == 'animated':
        print("🎬 Creating animated developmental visualization...")
        visualizer.create_animated_visualization()
    elif args.mode == 'report':
        print("📋 Generating milestone achievement report...")
        visualizer.generate_milestone_report()

if __name__ == "__main__":
    main()