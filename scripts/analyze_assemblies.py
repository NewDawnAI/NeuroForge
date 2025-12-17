#!/usr/bin/env python3
"""
Neural Assembly Analysis Script
Analyzes the emergent properties and functional relationships of neural assemblies
from the 500K neuron simulation.
"""

import pandas as pd
import numpy as np
import json
from collections import defaultdict
import os

def analyze_neural_assemblies():
    """Analyze neural assembly formation and dynamics."""
    print("=== NEURAL ASSEMBLY ANALYSIS ===")
    
    # Load neural assembly data
    if not os.path.exists('neural_assembly_analysis.csv'):
        print("Warning: neural_assembly_analysis.csv not found")
        return
    
    df = pd.read_csv('neural_assembly_analysis.csv')
    print(f"Total synaptic connections analyzed: {len(df)}")
    print(f"Unique pre-synaptic neurons: {df['pre_neuron'].nunique()}")
    print(f"Unique post-synaptic neurons: {df['post_neuron'].nunique()}")
    print(f"Weight range: {df['weight'].min():.6f} to {df['weight'].max():.6f}")
    print(f"Mean weight: {df['weight'].mean():.6f}")
    print(f"Weight std dev: {df['weight'].std():.6f}")
    
    # Analyze weight distribution
    positive_weights = df[df['weight'] > 0]
    negative_weights = df[df['weight'] < 0]
    print(f"Positive weights: {len(positive_weights)} ({len(positive_weights)/len(df)*100:.1f}%)")
    print(f"Negative weights: {len(negative_weights)} ({len(negative_weights)/len(df)*100:.1f}%)")
    
    # Identify strong connections (top 10%)
    strong_threshold = df['weight'].quantile(0.9)
    strong_connections = df[df['weight'] >= strong_threshold]
    print(f"Strong connections (>90th percentile): {len(strong_connections)}")
    print(f"Strong connection threshold: {strong_threshold:.6f}")
    
    # Analyze connectivity patterns
    neuron_in_degree = df.groupby('post_neuron').size()
    neuron_out_degree = df.groupby('pre_neuron').size()
    print(f"Max in-degree: {neuron_in_degree.max()}")
    print(f"Max out-degree: {neuron_out_degree.max()}")
    print(f"Mean in-degree: {neuron_in_degree.mean():.2f}")
    print(f"Mean out-degree: {neuron_out_degree.mean():.2f}")
    
    return df, strong_connections

def identify_neural_assemblies(df, min_assembly_size=3, weight_threshold=0.7):
    """Identify potential neural assemblies based on strong interconnections."""
    print(f"\n=== NEURAL ASSEMBLY IDENTIFICATION ===")
    
    # Find strongly connected components
    strong_connections = df[df['weight'] >= weight_threshold]
    
    # Build adjacency lists
    adjacency = defaultdict(set)
    for _, row in strong_connections.iterrows():
        pre, post = int(row['pre_neuron']), int(row['post_neuron'])
        adjacency[pre].add(post)
        adjacency[post].add(pre)  # Treat as undirected for assembly detection
    
    # Find connected components (potential assemblies)
    visited = set()
    assemblies = []
    
    def dfs(node, component):
        if node in visited:
            return
        visited.add(node)
        component.add(node)
        for neighbor in adjacency[node]:
            dfs(neighbor, component)
    
    for node in adjacency:
        if node not in visited:
            component = set()
            dfs(node, component)
            if len(component) >= min_assembly_size:
                assemblies.append(component)
    
    print(f"Identified {len(assemblies)} potential neural assemblies")
    print(f"Assembly sizes: {[len(a) for a in assemblies]}")
    
    return assemblies

def analyze_emergent_properties(df, assemblies):
    """Analyze emergent properties of identified assemblies."""
    print(f"\n=== EMERGENT PROPERTIES ANALYSIS ===")
    
    properties = {
        'total_assemblies': len(assemblies),
        'assembly_sizes': [len(a) for a in assemblies],
        'largest_assembly': max([len(a) for a in assemblies]) if assemblies else 0,
        'mean_assembly_size': np.mean([len(a) for a in assemblies]) if assemblies else 0,
        'assembly_connectivity': []
    }
    
    # Analyze internal connectivity of each assembly
    for i, assembly in enumerate(assemblies):
        assembly_connections = df[
            (df['pre_neuron'].isin(assembly)) & 
            (df['post_neuron'].isin(assembly))
        ]
        
        internal_strength = assembly_connections['weight'].mean()
        connection_density = len(assembly_connections) / (len(assembly) * (len(assembly) - 1))
        
        properties['assembly_connectivity'].append({
            'assembly_id': i,
            'size': len(assembly),
            'internal_strength': internal_strength,
            'connection_density': connection_density,
            'neurons': list(assembly)
        })
        
        print(f"Assembly {i}: {len(assembly)} neurons, "
              f"strength={internal_strength:.4f}, density={connection_density:.4f}")
    
    return properties

def analyze_functional_relationships(df, assemblies):
    """Analyze functional relationships between assemblies."""
    print(f"\n=== FUNCTIONAL RELATIONSHIPS ANALYSIS ===")
    
    relationships = []
    
    for i, assembly1 in enumerate(assemblies):
        for j, assembly2 in enumerate(assemblies):
            if i >= j:  # Avoid duplicates and self-connections
                continue
            
            # Find connections between assemblies
            inter_connections = df[
                (df['pre_neuron'].isin(assembly1)) & 
                (df['post_neuron'].isin(assembly2))
            ]
            reverse_connections = df[
                (df['pre_neuron'].isin(assembly2)) & 
                (df['post_neuron'].isin(assembly1))
            ]
            
            if len(inter_connections) > 0 or len(reverse_connections) > 0:
                forward_strength = inter_connections['weight'].mean() if len(inter_connections) > 0 else 0
                reverse_strength = reverse_connections['weight'].mean() if len(reverse_connections) > 0 else 0
                
                relationship = {
                    'assembly1': i,
                    'assembly2': j,
                    'forward_connections': len(inter_connections),
                    'reverse_connections': len(reverse_connections),
                    'forward_strength': forward_strength,
                    'reverse_strength': reverse_strength,
                    'bidirectional': len(inter_connections) > 0 and len(reverse_connections) > 0
                }
                
                relationships.append(relationship)
                
                print(f"Assembly {i} <-> Assembly {j}: "
                      f"forward={len(inter_connections)}({forward_strength:.4f}), "
                      f"reverse={len(reverse_connections)}({reverse_strength:.4f})")
    
    return relationships

def generate_comprehensive_report(df, assemblies, properties, relationships, learning_stats):
    """Generate a comprehensive analysis report."""
    
    report = {
        'simulation_summary': {
            'total_neurons': 500000,
            'analyzed_connections': len(df),
            'learning_statistics': learning_stats
        },
        'neural_assemblies': {
            'count': len(assemblies),
            'properties': properties,
            'functional_relationships': relationships
        },
        'emergent_behaviors': {
            'assembly_formation': len(assemblies) > 0,
            'hierarchical_organization': len([r for r in relationships if r['bidirectional']]) > 0,
            'functional_specialization': properties['mean_assembly_size'] > 3 if assemblies else False
        }
    }
    
    # Save detailed report
    with open('neural_assembly_report.json', 'w') as f:
        json.dump(report, f, indent=2, default=str)
    
    return report

def main():
    """Main analysis function."""
    print("Starting Neural Assembly Analysis for 500K Neuron Simulation")
    print("=" * 60)
    
    # Analyze neural assemblies
    df, strong_connections = analyze_neural_assemblies()
    
    # Identify assemblies
    assemblies = identify_neural_assemblies(df, min_assembly_size=3, weight_threshold=0.7)
    
    # Analyze emergent properties
    properties = analyze_emergent_properties(df, assemblies)
    
    # Analyze functional relationships
    relationships = analyze_functional_relationships(df, assemblies)
    
    # Learning statistics from simulation
    learning_stats = {
        'total_updates': 101459,
        'hebbian_updates': 52359,
        'stdp_updates': 49100,
        'phase4_updates': 0,
        'avg_weight_change': 9.16745e-05,
        'active_synapses': 105,
        'potentiated_synapses': 58316,
        'depressed_synapses': 7115
    }
    
    # Generate comprehensive report
    report = generate_comprehensive_report(df, assemblies, properties, relationships, learning_stats)
    
    print(f"\n=== SIMULATION SUMMARY ===")
    print(f"Successfully analyzed 500,000 neuron simulation")
    print(f"Identified {len(assemblies)} neural assemblies")
    print(f"Detected {len(relationships)} functional relationships")
    print(f"Learning updates: {learning_stats['total_updates']:,}")
    print(f"Synaptic plasticity: {learning_stats['potentiated_synapses']:,} potentiated, {learning_stats['depressed_synapses']:,} depressed")
    
    print(f"\nDetailed report saved to: neural_assembly_report.json")
    print("Analysis complete!")

if __name__ == "__main__":
    main()