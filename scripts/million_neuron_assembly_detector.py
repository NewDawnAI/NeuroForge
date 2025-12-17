#!/usr/bin/env python3
"""
NeuroForge Million Neuron Assembly Detector
Optimized for large-scale neural assembly detection (1M+ neurons)
"""

import numpy as np
import pandas as pd
from sklearn.cluster import DBSCAN
from sklearn.neighbors import NearestNeighbors
import matplotlib.pyplot as plt
import seaborn as sns
import json
import time
import gc
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import warnings
warnings.filterwarnings('ignore')

class MillionNeuronAssemblyDetector:
    """Optimized assembly detector for million-neuron scale networks."""
    
    def __init__(self, connectivity_threshold: float = 0.05, min_assembly_size: int = 5, max_assembly_size: int = 1000):
        self.connectivity_threshold = connectivity_threshold
        self.min_assembly_size = min_assembly_size
        self.max_assembly_size = max_assembly_size
        self.assemblies = []
        self.neuron_ids = None
        self.connectivity_data = None
        self.chunk_size = 100000  # Process in chunks to manage memory
        
    def load_connectivity_data_chunked(self, filepath: str) -> bool:
        """Load connectivity data in chunks to handle large files."""
        try:
            print(f"Loading connectivity data from: {filepath}")
            start_time = time.time()
            
            # First pass: get unique neuron IDs and count connections
            print("  First pass: analyzing file structure...")
            chunk_iter = pd.read_csv(filepath, chunksize=self.chunk_size)
            
            all_source_ids = set()
            all_target_ids = set()
            total_connections = 0
            
            for chunk_idx, chunk in enumerate(chunk_iter):
                # Handle different column naming conventions
                if 'pre_neuron' in chunk.columns and 'post_neuron' in chunk.columns:
                    chunk = chunk.rename(columns={'pre_neuron': 'source_id', 'post_neuron': 'target_id'})
                
                if not all(col in chunk.columns for col in ['source_id', 'target_id', 'weight']):
                    print(f"Error: Invalid column format in chunk {chunk_idx}")
                    return False
                
                # Filter by weight threshold
                chunk = chunk[abs(chunk['weight']) >= self.connectivity_threshold]
                
                all_source_ids.update(chunk['source_id'].tolist())
                all_target_ids.update(chunk['target_id'].tolist())
                total_connections += len(chunk)
                
                if chunk_idx % 10 == 0:
                    print(f"    Processed chunk {chunk_idx}, connections so far: {total_connections}")
            
            # Create neuron ID mapping
            self.neuron_ids = sorted(list(all_source_ids.union(all_target_ids)))
            n_neurons = len(self.neuron_ids)
            id_to_idx = {nid: idx for idx, nid in enumerate(self.neuron_ids)}
            
            print(f"  Found {n_neurons} neurons with {total_connections} connections above threshold")
            
            # Second pass: build sparse connectivity representation
            print("  Second pass: building connectivity representation...")
            
            # Use list of connections instead of full matrix for memory efficiency
            connections = []
            chunk_iter = pd.read_csv(filepath, chunksize=self.chunk_size)
            
            for chunk_idx, chunk in enumerate(chunk_iter):
                if 'pre_neuron' in chunk.columns and 'post_neuron' in chunk.columns:
                    chunk = chunk.rename(columns={'pre_neuron': 'source_id', 'post_neuron': 'target_id'})
                
                chunk = chunk[abs(chunk['weight']) >= self.connectivity_threshold]
                
                for _, row in chunk.iterrows():
                    src_idx = id_to_idx[row['source_id']]
                    tgt_idx = id_to_idx[row['target_id']]
                    weight = abs(row['weight'])
                    connections.append((src_idx, tgt_idx, weight))
                
                if chunk_idx % 10 == 0:
                    print(f"    Processed chunk {chunk_idx}")
            
            self.connectivity_data = connections
            load_time = time.time() - start_time
            
            print(f"  Data loaded successfully in {load_time:.1f} seconds")
            print(f"  Neurons: {n_neurons}, Connections: {len(connections)}")
            
            return True
            
        except Exception as e:
            print(f"Error loading connectivity data: {e}")
            return False
    
    def build_neighbor_graph(self) -> Dict[int, List[Tuple[int, float]]]:
        """Build neighbor graph for efficient clustering."""
        print("Building neighbor graph...")
        start_time = time.time()
        
        neighbor_graph = {i: [] for i in range(len(self.neuron_ids))}
        
        for src_idx, tgt_idx, weight in self.connectivity_data:
            neighbor_graph[src_idx].append((tgt_idx, weight))
            neighbor_graph[tgt_idx].append((src_idx, weight))  # Make undirected
        
        # Sort neighbors by weight (strongest first)
        for neuron_idx in neighbor_graph:
            neighbor_graph[neuron_idx].sort(key=lambda x: x[1], reverse=True)
        
        build_time = time.time() - start_time
        print(f"  Neighbor graph built in {build_time:.1f} seconds")
        
        return neighbor_graph
    
    def detect_assemblies_optimized_dbscan(self, eps: float = 0.1, min_samples: int = 5) -> List[Dict]:
        """Optimized DBSCAN for large-scale assembly detection."""
        if not self.connectivity_data:
            print("Error: No connectivity data loaded")
            return []
        
        print(f"Detecting assemblies using optimized DBSCAN (eps={eps}, min_samples={min_samples})...")
        start_time = time.time()
        
        # Build feature matrix for clustering (use connectivity patterns)
        print("  Building feature matrix...")
        n_neurons = len(self.neuron_ids)
        
        # Use sparse representation - for each neuron, create feature vector of top connections
        max_features = min(100, n_neurons // 100)  # Limit features for memory efficiency
        feature_matrix = np.zeros((n_neurons, max_features))
        
        neighbor_graph = self.build_neighbor_graph()
        
        for neuron_idx in range(n_neurons):
            neighbors = neighbor_graph[neuron_idx][:max_features]  # Top connections only
            for i, (neighbor_idx, weight) in enumerate(neighbors):
                if i < max_features:
                    feature_matrix[neuron_idx, i] = weight
        
        print(f"  Feature matrix shape: {feature_matrix.shape}")
        
        # Apply DBSCAN clustering
        print("  Running DBSCAN clustering...")
        clustering = DBSCAN(eps=eps, min_samples=min_samples, metric='euclidean', n_jobs=-1)
        labels = clustering.fit_predict(feature_matrix)
        
        # Process results
        assemblies = []
        unique_labels = set(labels)
        unique_labels.discard(-1)  # Remove noise label
        
        print(f"  Found {len(unique_labels)} potential assemblies")
        
        for label in unique_labels:
            assembly_indices = np.where(labels == label)[0]
            assembly_neurons = [self.neuron_ids[idx] for idx in assembly_indices]
            
            if self.min_assembly_size <= len(assembly_neurons) <= self.max_assembly_size:
                # Calculate assembly statistics
                internal_connections = 0
                external_connections = 0
                total_weight = 0
                
                # Count internal vs external connections
                assembly_set = set(assembly_indices)
                for neuron_idx in assembly_indices:
                    for neighbor_idx, weight in neighbor_graph[neuron_idx]:
                        if neighbor_idx in assembly_set:
                            internal_connections += 1
                            total_weight += weight
                        else:
                            external_connections += 1
                
                # Avoid division by zero
                internal_strength = total_weight / max(internal_connections, 1)
                cohesion = internal_connections / max(external_connections, 1)
                
                assembly = {
                    'id': len(assemblies),
                    'neurons': [int(nid) for nid in assembly_neurons],
                    'size': len(assembly_neurons),
                    'internal_strength': float(internal_strength),
                    'external_connections': external_connections,
                    'internal_connections': internal_connections,
                    'cohesion': float(cohesion),
                    'method': 'Optimized_DBSCAN'
                }
                assemblies.append(assembly)
        
        # Clean up memory
        del feature_matrix
        del neighbor_graph
        gc.collect()
        
        detection_time = time.time() - start_time
        print(f"  Assembly detection completed in {detection_time:.1f} seconds")
        print(f"  Detected {len(assemblies)} valid assemblies")
        
        self.assemblies = assemblies
        return assemblies
    
    def analyze_assembly_statistics(self) -> Dict:
        """Analyze statistical properties of detected assemblies."""
        if not self.assemblies:
            return {}
        
        print("Analyzing assembly statistics...")
        
        sizes = [a['size'] for a in self.assemblies]
        cohesions = [a['cohesion'] for a in self.assemblies]
        internal_strengths = [a['internal_strength'] for a in self.assemblies]
        
        stats = {
            'total_assemblies': len(self.assemblies),
            'total_neurons': len(self.neuron_ids),
            'neurons_in_assemblies': sum(sizes),
            'coverage_percentage': (sum(sizes) / len(self.neuron_ids)) * 100,
            'size_statistics': {
                'mean': float(np.mean(sizes)),
                'std': float(np.std(sizes)),
                'min': int(np.min(sizes)),
                'max': int(np.max(sizes)),
                'median': float(np.median(sizes))
            },
            'cohesion_statistics': {
                'mean': float(np.mean(cohesions)),
                'std': float(np.std(cohesions)),
                'min': float(np.min(cohesions)),
                'max': float(np.max(cohesions)),
                'median': float(np.median(cohesions))
            },
            'strength_statistics': {
                'mean': float(np.mean(internal_strengths)),
                'std': float(np.std(internal_strengths)),
                'min': float(np.min(internal_strengths)),
                'max': float(np.max(internal_strengths)),
                'median': float(np.median(internal_strengths))
            }
        }
        
        return stats
    
    def generate_report(self, output_file: str) -> Dict:
        """Generate comprehensive assembly analysis report."""
        print(f"Generating analysis report: {output_file}")
        
        stats = self.analyze_assembly_statistics()
        
        report = {
            'test_info': {
                'total_neurons': len(self.neuron_ids) if self.neuron_ids else 0,
                'connectivity_threshold': self.connectivity_threshold,
                'min_assembly_size': self.min_assembly_size,
                'max_assembly_size': self.max_assembly_size,
                'total_connections': len(self.connectivity_data) if self.connectivity_data else 0
            },
            'assembly_statistics': stats,
            'assemblies': self.assemblies[:100]  # Limit to first 100 for file size
        }
        
        # Save report
        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"  Report saved with {len(self.assemblies)} assemblies")
        return report
    
    def visualize_assemblies(self, output_file: str, figsize: Tuple[int, int] = (16, 12)):
        """Create visualizations of assembly analysis."""
        if not self.assemblies:
            print("No assemblies to visualize")
            return
        
        print(f"Creating assembly visualizations: {output_file}")
        
        fig, axes = plt.subplots(2, 3, figsize=figsize)
        fig.suptitle(f'Million Neuron Assembly Analysis ({len(self.assemblies)} assemblies)', fontsize=16)
        
        # 1. Assembly size distribution
        sizes = [a['size'] for a in self.assemblies]
        axes[0, 0].hist(sizes, bins=min(50, len(set(sizes))), alpha=0.7, color='skyblue', edgecolor='black')
        axes[0, 0].set_xlabel('Assembly Size (neurons)')
        axes[0, 0].set_ylabel('Frequency')
        axes[0, 0].set_title('Assembly Size Distribution')
        axes[0, 0].grid(True, alpha=0.3)
        
        # 2. Cohesion distribution
        cohesions = [a['cohesion'] for a in self.assemblies]
        axes[0, 1].hist(cohesions, bins=30, alpha=0.7, color='lightgreen', edgecolor='black')
        axes[0, 1].set_xlabel('Cohesion Score')
        axes[0, 1].set_ylabel('Frequency')
        axes[0, 1].set_title('Assembly Cohesion Distribution')
        axes[0, 1].grid(True, alpha=0.3)
        
        # 3. Internal strength distribution
        strengths = [a['internal_strength'] for a in self.assemblies]
        axes[0, 2].hist(strengths, bins=30, alpha=0.7, color='salmon', edgecolor='black')
        axes[0, 2].set_xlabel('Internal Strength')
        axes[0, 2].set_ylabel('Frequency')
        axes[0, 2].set_title('Internal Strength Distribution')
        axes[0, 2].grid(True, alpha=0.3)
        
        # 4. Size vs Cohesion scatter
        axes[1, 0].scatter(sizes, cohesions, alpha=0.6, s=20)
        axes[1, 0].set_xlabel('Assembly Size')
        axes[1, 0].set_ylabel('Cohesion Score')
        axes[1, 0].set_title('Size vs Cohesion')
        axes[1, 0].grid(True, alpha=0.3)
        
        # 5. Assembly size categories
        size_categories = {'Small (5-20)': 0, 'Medium (21-100)': 0, 'Large (101-500)': 0, 'Mega (500+)': 0}
        for size in sizes:
            if size <= 20:
                size_categories['Small (5-20)'] += 1
            elif size <= 100:
                size_categories['Medium (21-100)'] += 1
            elif size <= 500:
                size_categories['Large (101-500)'] += 1
            else:
                size_categories['Mega (500+)'] += 1
        
        axes[1, 1].bar(size_categories.keys(), size_categories.values(), color=['lightblue', 'lightgreen', 'orange', 'red'])
        axes[1, 1].set_ylabel('Count')
        axes[1, 1].set_title('Assembly Size Categories')
        axes[1, 1].tick_params(axis='x', rotation=45)
        
        # 6. Coverage statistics
        stats = self.analyze_assembly_statistics()
        coverage_data = {
            'In Assemblies': stats.get('neurons_in_assemblies', 0),
            'Not in Assemblies': stats.get('total_neurons', 0) - stats.get('neurons_in_assemblies', 0)
        }
        
        axes[1, 2].pie(coverage_data.values(), labels=coverage_data.keys(), autopct='%1.1f%%', startangle=90)
        axes[1, 2].set_title(f'Neuron Coverage\n({stats.get("coverage_percentage", 0):.1f}% in assemblies)')
        
        plt.tight_layout()
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        plt.close()
        
        print(f"  Visualization saved: {output_file}")
    
    def print_summary(self):
        """Print a summary of detected assemblies."""
        if not self.assemblies:
            print("No assemblies detected")
            return
        
        stats = self.analyze_assembly_statistics()
        
        print("\n" + "="*60)
        print("MILLION NEURON ASSEMBLY DETECTION SUMMARY")
        print("="*60)
        print(f"Total Neurons: {stats['total_neurons']:,}")
        print(f"Total Assemblies: {stats['total_assemblies']}")
        print(f"Neurons in Assemblies: {stats['neurons_in_assemblies']:,}")
        print(f"Coverage: {stats['coverage_percentage']:.1f}%")
        print()
        print("Assembly Size Statistics:")
        print(f"  Mean: {stats['size_statistics']['mean']:.1f}")
        print(f"  Range: {stats['size_statistics']['min']} - {stats['size_statistics']['max']}")
        print(f"  Median: {stats['size_statistics']['median']:.1f}")
        print()
        print("Cohesion Statistics:")
        print(f"  Mean: {stats['cohesion_statistics']['mean']:.2f}")
        print(f"  Range: {stats['cohesion_statistics']['min']:.2f} - {stats['cohesion_statistics']['max']:.2f}")
        print()
        print("Top 10 Largest Assemblies:")
        sorted_assemblies = sorted(self.assemblies, key=lambda x: x['size'], reverse=True)
        for i, assembly in enumerate(sorted_assemblies[:10]):
            print(f"  {i+1:2d}. Size: {assembly['size']:4d}, Cohesion: {assembly['cohesion']:6.2f}, Strength: {assembly['internal_strength']:.3f}")
        print("="*60)

def main():
    """Main function for command-line usage."""
    import argparse
    
    parser = argparse.ArgumentParser(description='Million Neuron Assembly Detector')
    parser.add_argument('connectivity_file', help='Path to connectivity CSV file')
    parser.add_argument('--output-dir', default='million_neuron_results', help='Output directory')
    parser.add_argument('--threshold', type=float, default=0.05, help='Connectivity threshold')
    parser.add_argument('--min-size', type=int, default=5, help='Minimum assembly size')
    parser.add_argument('--max-size', type=int, default=1000, help='Maximum assembly size')
    parser.add_argument('--eps', type=float, default=0.1, help='DBSCAN eps parameter')
    parser.add_argument('--min-samples', type=int, default=5, help='DBSCAN min_samples parameter')
    
    args = parser.parse_args()
    
    # Create output directory
    Path(args.output_dir).mkdir(exist_ok=True)
    
    # Initialize detector
    detector = MillionNeuronAssemblyDetector(
        connectivity_threshold=args.threshold,
        min_assembly_size=args.min_size,
        max_assembly_size=args.max_size
    )
    
    # Load data and detect assemblies
    if detector.load_connectivity_data_chunked(args.connectivity_file):
        assemblies = detector.detect_assemblies_optimized_dbscan(eps=args.eps, min_samples=args.min_samples)
        
        if assemblies:
            # Generate outputs
            report_file = Path(args.output_dir) / 'million_neuron_assembly_report.json'
            viz_file = Path(args.output_dir) / 'million_neuron_assemblies.png'
            
            detector.generate_report(str(report_file))
            detector.visualize_assemblies(str(viz_file))
            detector.print_summary()
        else:
            print("No assemblies detected with current parameters")
    else:
        print("Failed to load connectivity data")

if __name__ == '__main__':
    main()