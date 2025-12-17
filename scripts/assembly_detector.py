#!/usr/bin/env python3
"""
NeuroForge Assembly Detection Tool
Analyzes neural substrate snapshots to identify emergent assemblies and higher-order patterns.
"""

import numpy as np
import pandas as pd
import networkx as nx
from sklearn.cluster import DBSCAN, SpectralClustering
from sklearn.metrics import silhouette_score
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
import argparse
import json
from typing import Dict, List, Tuple, Optional
import warnings
warnings.filterwarnings('ignore')

class AssemblyDetector:
    """Detects and analyzes neural assemblies from connectivity data."""
    
    def __init__(self, connectivity_threshold: float = 0.1, min_assembly_size: int = 3):
        self.connectivity_threshold = connectivity_threshold
        self.min_assembly_size = min_assembly_size
        self.assemblies = []
        self.connectivity_matrix = None
        self.neuron_ids = None
        
    def load_connectivity_data(self, filepath: str) -> bool:
        """Load connectivity data from CSV file."""
        try:
            # Expected format: pre_neuron, post_neuron, weight OR source_id, target_id, weight
            df = pd.read_csv(filepath)
            
            # Check for different column naming conventions
            if 'pre_neuron' in df.columns and 'post_neuron' in df.columns and 'weight' in df.columns:
                df = df.rename(columns={'pre_neuron': 'source_id', 'post_neuron': 'target_id'})
            elif not all(col in df.columns for col in ['source_id', 'target_id', 'weight']):
                print(f"Error: CSV must contain columns: source_id, target_id, weight OR pre_neuron, post_neuron, weight")
                print(f"Found columns: {list(df.columns)}")
                return False
                
            # Get unique neuron IDs
            self.neuron_ids = sorted(list(set(df['source_id'].tolist() + df['target_id'].tolist())))
            n_neurons = len(self.neuron_ids)
            
            # Create neuron ID to index mapping
            id_to_idx = {nid: idx for idx, nid in enumerate(self.neuron_ids)}
            
            # Build connectivity matrix
            self.connectivity_matrix = np.zeros((n_neurons, n_neurons))
            
            for _, row in df.iterrows():
                src_idx = id_to_idx[row['source_id']]
                tgt_idx = id_to_idx[row['target_id']]
                weight = abs(row['weight'])  # Use absolute weight for connectivity strength
                
                self.connectivity_matrix[src_idx, tgt_idx] = weight
                # Make symmetric for undirected analysis
                self.connectivity_matrix[tgt_idx, src_idx] = weight
                
            print(f"Loaded connectivity data: {n_neurons} neurons, {len(df)} connections")
            return True
            
        except Exception as e:
            print(f"Error loading connectivity data: {e}")
            return False
    
    def detect_assemblies_dbscan(self, eps: float = 0.3, min_samples: int = 3) -> List[Dict]:
        """Detect assemblies using DBSCAN clustering on connectivity patterns."""
        if self.connectivity_matrix is None:
            print("Error: No connectivity data loaded")
            return []
            
        # Use connectivity matrix as feature space
        features = self.connectivity_matrix
        
        # Apply DBSCAN clustering
        clustering = DBSCAN(eps=eps, min_samples=min_samples, metric='euclidean')
        labels = clustering.fit_predict(features)
        
        assemblies = []
        unique_labels = set(labels)
        
        for label in unique_labels:
            if label == -1:  # Noise points
                continue
                
            # Get neurons in this assembly
            assembly_indices = np.where(labels == label)[0]
            assembly_neurons = [self.neuron_ids[idx] for idx in assembly_indices]
            
            if len(assembly_neurons) >= self.min_assembly_size:
                # Calculate assembly statistics
                assembly_connectivity = self.connectivity_matrix[np.ix_(assembly_indices, assembly_indices)]
                internal_strength = np.mean(assembly_connectivity[assembly_connectivity > 0])
                
                # Calculate external connectivity
                external_indices = [i for i in range(len(self.neuron_ids)) if i not in assembly_indices]
                if external_indices:
                    external_connectivity = self.connectivity_matrix[np.ix_(assembly_indices, external_indices)]
                    external_strength = np.mean(external_connectivity[external_connectivity > 0]) if np.any(external_connectivity > 0) else 0
                else:
                    external_strength = 0
                
                assembly = {
                    'id': len(assemblies),
                    'neurons': [int(nid) for nid in assembly_neurons],  # Convert to int
                    'size': int(len(assembly_neurons)),  # Convert to int
                    'internal_strength': float(internal_strength),  # Convert to float
                    'external_strength': float(external_strength),  # Convert to float
                    'cohesion': float(internal_strength / (external_strength + 1e-6)),  # Convert to float
                    'method': 'DBSCAN'
                }
                assemblies.append(assembly)
        
        self.assemblies = assemblies
        print(f"Detected {len(assemblies)} assemblies using DBSCAN")
        return assemblies
    
    def detect_assemblies_spectral(self, n_clusters: int = None) -> List[Dict]:
        """Detect assemblies using spectral clustering on connectivity graph."""
        if self.connectivity_matrix is None:
            print("Error: No connectivity data loaded")
            return []
        
        # Threshold connectivity matrix to create binary adjacency
        adjacency = (self.connectivity_matrix > self.connectivity_threshold).astype(int)
        
        # Estimate number of clusters if not provided
        if n_clusters is None:
            # Use eigenvalue gap heuristic
            eigenvals = np.linalg.eigvals(adjacency)
            eigenvals = np.sort(eigenvals)[::-1]
            gaps = np.diff(eigenvals)
            n_clusters = np.argmax(gaps) + 2  # +2 because diff reduces size by 1 and we want 1-indexed
            n_clusters = min(max(n_clusters, 2), len(self.neuron_ids) // 2)  # Reasonable bounds
        
        # Apply spectral clustering
        clustering = SpectralClustering(n_clusters=n_clusters, affinity='precomputed', random_state=42)
        labels = clustering.fit_predict(adjacency)
        
        assemblies = []
        unique_labels = set(labels)
        
        for label in unique_labels:
            # Get neurons in this assembly
            assembly_indices = np.where(labels == label)[0]
            assembly_neurons = [self.neuron_ids[idx] for idx in assembly_indices]
            
            if len(assembly_neurons) >= self.min_assembly_size:
                # Calculate assembly statistics
                assembly_connectivity = self.connectivity_matrix[np.ix_(assembly_indices, assembly_indices)]
                internal_strength = np.mean(assembly_connectivity[assembly_connectivity > 0])
                
                # Calculate external connectivity
                external_indices = [i for i in range(len(self.neuron_ids)) if i not in assembly_indices]
                if external_indices:
                    external_connectivity = self.connectivity_matrix[np.ix_(assembly_indices, external_indices)]
                    external_strength = np.mean(external_connectivity[external_connectivity > 0]) if np.any(external_connectivity > 0) else 0
                else:
                    external_strength = 0
                
                assembly = {
                    'id': len(assemblies),
                    'neurons': [int(nid) for nid in assembly_neurons],  # Convert to int
                    'size': int(len(assembly_neurons)),  # Convert to int
                    'internal_strength': float(internal_strength),  # Convert to float
                    'external_strength': float(external_strength),  # Convert to float
                    'cohesion': float(internal_strength / (external_strength + 1e-6)),  # Convert to float
                    'method': 'Spectral'
                }
                assemblies.append(assembly)
        
        self.assemblies = assemblies
        print(f"Detected {len(assemblies)} assemblies using Spectral Clustering")
        return assemblies
    
    def analyze_assembly_hierarchy(self) -> Dict:
        """Analyze hierarchical relationships between assemblies."""
        if not self.assemblies:
            return {}
        
        # Build assembly-to-assembly connectivity
        n_assemblies = len(self.assemblies)
        assembly_connectivity = np.zeros((n_assemblies, n_assemblies))
        
        for i, assembly_a in enumerate(self.assemblies):
            for j, assembly_b in enumerate(self.assemblies):
                if i != j:
                    # Calculate connectivity between assemblies
                    neurons_a = [self.neuron_ids.index(nid) for nid in assembly_a['neurons']]
                    neurons_b = [self.neuron_ids.index(nid) for nid in assembly_b['neurons']]
                    
                    inter_connectivity = self.connectivity_matrix[np.ix_(neurons_a, neurons_b)]
                    assembly_connectivity[i, j] = np.mean(inter_connectivity[inter_connectivity > 0]) if np.any(inter_connectivity > 0) else 0
        
        # Identify hierarchical relationships
        hierarchy = {
            'assembly_connectivity': assembly_connectivity.tolist(),
            'strong_connections': [],
            'potential_superassemblies': []
        }
        
        # Find strong inter-assembly connections
        threshold = np.percentile(assembly_connectivity[assembly_connectivity > 0], 75) if np.any(assembly_connectivity > 0) else 0
        strong_pairs = np.where(assembly_connectivity > threshold)
        
        for i, j in zip(strong_pairs[0], strong_pairs[1]):
            hierarchy['strong_connections'].append({
                'assembly_a': int(i),
                'assembly_b': int(j),
                'strength': float(assembly_connectivity[i, j])
            })
        
        return hierarchy
    
    def generate_report(self, output_path: str = "assembly_report.json") -> Dict:
        """Generate comprehensive assembly analysis report."""
        if not self.assemblies:
            print("No assemblies detected. Run detection first.")
            return {}
        
        # Basic statistics
        sizes = [a['size'] for a in self.assemblies]
        cohesions = [a['cohesion'] for a in self.assemblies]
        
        report = {
            'summary': {
                'total_assemblies': int(len(self.assemblies)),
                'total_neurons': int(len(self.neuron_ids)),
                'neurons_in_assemblies': int(sum(sizes)),
                'coverage_percentage': float((sum(sizes) / len(self.neuron_ids)) * 100),
                'avg_assembly_size': float(np.mean(sizes)),
                'max_assembly_size': int(max(sizes)),
                'min_assembly_size': int(min(sizes)),
                'avg_cohesion': float(np.mean(cohesions)),
                'max_cohesion': float(max(cohesions))
            },
            'assemblies': self.assemblies,
            'hierarchy': self.analyze_assembly_hierarchy(),
            'parameters': {
                'connectivity_threshold': float(self.connectivity_threshold),
                'min_assembly_size': int(self.min_assembly_size)
            }
        }
        
        # Save report
        with open(output_path, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"Assembly analysis report saved to: {output_path}")
        return report
    
    def visualize_assemblies(self, output_path: str = "assembly_visualization.png", figsize: Tuple[int, int] = (12, 8)):
        """Create visualization of detected assemblies."""
        if not self.assemblies:
            print("No assemblies detected. Run detection first.")
            return
        
        fig, axes = plt.subplots(2, 2, figsize=figsize)
        fig.suptitle('Neural Assembly Analysis', fontsize=16)
        
        # 1. Assembly size distribution
        sizes = [a['size'] for a in self.assemblies]
        axes[0, 0].hist(sizes, bins=max(1, len(set(sizes))), alpha=0.7, color='skyblue')
        axes[0, 0].set_title('Assembly Size Distribution')
        axes[0, 0].set_xlabel('Assembly Size (neurons)')
        axes[0, 0].set_ylabel('Count')
        
        # 2. Cohesion distribution
        cohesions = [a['cohesion'] for a in self.assemblies]
        axes[0, 1].hist(cohesions, bins=20, alpha=0.7, color='lightgreen')
        axes[0, 1].set_title('Assembly Cohesion Distribution')
        axes[0, 1].set_xlabel('Cohesion (internal/external strength)')
        axes[0, 1].set_ylabel('Count')
        
        # 3. Connectivity matrix heatmap
        if self.connectivity_matrix is not None:
            im = axes[1, 0].imshow(self.connectivity_matrix, cmap='viridis', aspect='auto')
            axes[1, 0].set_title('Connectivity Matrix')
            axes[1, 0].set_xlabel('Neuron Index')
            axes[1, 0].set_ylabel('Neuron Index')
            plt.colorbar(im, ax=axes[1, 0])
        
        # 4. Assembly statistics scatter
        internal_strengths = [a['internal_strength'] for a in self.assemblies]
        external_strengths = [a['external_strength'] for a in self.assemblies]
        scatter = axes[1, 1].scatter(internal_strengths, external_strengths, 
                                   c=sizes, cmap='plasma', alpha=0.7, s=60)
        axes[1, 1].set_title('Assembly Internal vs External Strength')
        axes[1, 1].set_xlabel('Internal Strength')
        axes[1, 1].set_ylabel('External Strength')
        plt.colorbar(scatter, ax=axes[1, 1], label='Assembly Size')
        
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Assembly visualization saved to: {output_path}")

def main():
    parser = argparse.ArgumentParser(description='NeuroForge Assembly Detection Tool')
    parser.add_argument('input_file', help='Path to connectivity CSV file')
    parser.add_argument('--method', choices=['dbscan', 'spectral', 'both'], default='both',
                       help='Assembly detection method')
    parser.add_argument('--threshold', type=float, default=0.1,
                       help='Connectivity threshold for analysis')
    parser.add_argument('--min-size', type=int, default=3,
                       help='Minimum assembly size')
    parser.add_argument('--output-dir', default='assembly_analysis',
                       help='Output directory for results')
    parser.add_argument('--visualize', action='store_true',
                       help='Generate visualization plots')
    
    args = parser.parse_args()
    
    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(exist_ok=True)
    
    # Initialize detector
    detector = AssemblyDetector(
        connectivity_threshold=args.threshold,
        min_assembly_size=args.min_size
    )
    
    # Load data
    if not detector.load_connectivity_data(args.input_file):
        return 1
    
    # Run detection
    if args.method in ['dbscan', 'both']:
        assemblies_dbscan = detector.detect_assemblies_dbscan()
        report_dbscan = detector.generate_report(output_dir / 'assembly_report_dbscan.json')
        
        if args.visualize:
            detector.visualize_assemblies(output_dir / 'assembly_visualization_dbscan.png')
    
    if args.method in ['spectral', 'both']:
        assemblies_spectral = detector.detect_assemblies_spectral()
        report_spectral = detector.generate_report(output_dir / 'assembly_report_spectral.json')
        
        if args.visualize:
            detector.visualize_assemblies(output_dir / 'assembly_visualization_spectral.png')
    
    # Print summary
    print("\n=== Assembly Detection Summary ===")
    if args.method in ['dbscan', 'both']:
        print(f"DBSCAN: {len(assemblies_dbscan)} assemblies detected")
    if args.method in ['spectral', 'both']:
        print(f"Spectral: {len(assemblies_spectral)} assemblies detected")
    
    print(f"Results saved to: {output_dir}")
    return 0

if __name__ == '__main__':
    exit(main())