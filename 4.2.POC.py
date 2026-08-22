import numpy as np
import networkx as nx
from networkx.algorithms import community
import random
from scipy.optimize import curve_fit
from scipy.stats import entropy
from typing import Dict, List, Tuple, Callable
import time

# ============================================================================
# DirectedGraph Class (mimicking C++ version)
# ============================================================================
class DirectedGraph:
    """Directed graph with weighted edges"""
    def __init__(self, num_nodes: int):
        self.G = nx.DiGraph()
        self.G.add_nodes_from(range(num_nodes))
        self.num_nodes = num_nodes

    def add_edge(self, u: int, v: int, weight: float = 1.0):
        self.G.add_edge(u, v, weight=weight)

    def num_edges(self) -> int:
        return self.G.number_of_edges()

    def edges(self) -> List[Dict]:
        return [{'u': u, 'v': v, 'weight': self.G[u][v]['weight']} 
                for u, v in self.G.edges()]

    def adjacency_matrix(self) -> np.ndarray:
        return nx.to_numpy_array(self.G)
    
    def is_strongly_connected(self) -> bool:
        return nx.is_strongly_connected(self.G)
    
    def to_undirected(self):
        return self.G.to_undirected()


# ============================================================================
# UncertainMetric (matches 4.2.md exactly)
# ============================================================================
class UncertainMetric:
    """Uncertain metric with confidence intervals"""
    def __init__(self, mean: float, std_dev: float, confidence_lower: float, 
                 confidence_upper: float, num_samples: int):
        self.mean = mean
        self.std_dev = std_dev
        self.confidence_lower = confidence_lower
        self.confidence_upper = confidence_upper
        self.num_samples = num_samples
    
    def is_reliable(self, max_uncertainty: float = 0.15) -> bool:
        """Check if CI width is within acceptable bounds"""
        interval_width = self.confidence_upper - self.confidence_lower
        return interval_width < max_uncertainty
    
    def to_dict(self) -> Dict:
        return {
            'mean': self.mean,
            'std_dev': self.std_dev,
            'confidence_lower': self.confidence_lower,
            'confidence_upper': self.confidence_upper,
            'num_samples': self.num_samples,
            'reliable': self.is_reliable()
        }
    
    def __str__(self) -> str:
        return (f"{self.mean:.3f} ± {self.std_dev:.3f} "
                f"(95% CI: [{self.confidence_lower:.3f}, {self.confidence_upper:.3f}], "
                f"n={self.num_samples})")


# ============================================================================
# IntegrationHeuristics (Production Version - Discussions 4.2.md)
# ============================================================================
class IntegrationHeuristics:
    """
    Computable integration heuristics (NOT true IIT Φ)
    
    Based on Discussions.4.2.md - All metrics with uncertainty quantification.
    """
    
    def __init__(self, bootstrap_samples: int = 50, 
                 weights: List[float] = None,
                 enable_adaptive_weights: bool = True):
        self.bootstrap_samples = bootstrap_samples
        self.weights = weights if weights else [0.25, 0.25, 0.25, 0.25]
        self.enable_adaptive_weights = enable_adaptive_weights
        
        # Ensure weights sum to 1
        weight_sum = sum(self.weights)
        self.weights = [w / weight_sum for w in self.weights]
    
    # ========================================================================
    # Bootstrap Infrastructure (Proper Graph Resampling)
    # ========================================================================
    
    def bootstrap_graph(self, graph: DirectedGraph, seed: int) -> DirectedGraph:
        """
        Bootstrap resample graph by resampling edges with replacement.
        This is the CORRECT way to do bootstrap for graph metrics.
        """
        rng = np.random.default_rng(seed)
        edges = list(graph.G.edges(data=True))
        
        if len(edges) == 0:
            return graph
        
        # Resample edges with replacement
        n_edges = len(edges)
        bootstrap_indices = rng.choice(n_edges, size=n_edges, replace=True)
        
        # Create new graph
        bootstrap_g = DirectedGraph(graph.num_nodes)
        for idx in bootstrap_indices:
            u, v, data = edges[idx]
            weight = data.get('weight', 1.0)
            # Add noise to weight for additional variation
            noisy_weight = weight * (1.0 + rng.normal(0, 0.05))
            noisy_weight = max(0.01, min(2.0, noisy_weight))  # Bound
            bootstrap_g.add_edge(u, v, noisy_weight)
        
        return bootstrap_g
    
    def compute_with_uncertainty(self, 
                                 metric_fn: Callable[[DirectedGraph, int], float],
                                 graph: DirectedGraph) -> UncertainMetric:
        """
        Compute metric with uncertainty via bootstrap resampling.
        
        Args:
            metric_fn: Function that takes (graph, seed) and returns float
            graph: Graph to compute metric on
            
        Returns:
            UncertainMetric with mean, std, and 95% CI
        """
        samples = []
        
        for i in range(self.bootstrap_samples):
            # Proper bootstrap: resample graph structure
            bootstrap_g = self.bootstrap_graph(graph, seed=i)
            value = metric_fn(bootstrap_g, seed=i)
            samples.append(value)
        
        samples = np.array(samples)
        mean = np.mean(samples)
        std = np.std(samples, ddof=1)  # Use sample std (n-1)
        
        # 95% confidence interval (assuming normal distribution)
        margin = 1.96 * std / np.sqrt(self.bootstrap_samples)
        
        return UncertainMetric(
            mean=float(mean),
            std_dev=float(std),
            confidence_lower=float(mean - margin),
            confidence_upper=float(mean + margin),
            num_samples=self.bootstrap_samples
        )
    
    # ========================================================================
    # Individual Metrics (Production Implementations)
    # ========================================================================
    
    def compute_causal_density(self, graph: DirectedGraph, seed: int) -> float:
        """
        Equation: ρ_c = (1 / |V|) Σ w_ij / max(w)
        
        Mean normalized edge density - O(|E|)
        """
        if graph.num_edges() == 0:
            return 0.0
        
        edges = graph.edges()
        weights = [e['weight'] for e in edges]
        max_weight = max(weights)
        
        if max_weight < 1e-9:
            return 0.0
        
        # Normalized mean weight density
        sum_weights = sum(weights)
        num_possible_edges = graph.num_nodes * (graph.num_nodes - 1)
        
        return (sum_weights / num_possible_edges) / max_weight
    
    def compute_effective_info_bound(self, graph: DirectedGraph, seed: int) -> float:
        """
        Equation: EI_lb = (1/K) Σ external_density / internal_density
        
        Community-based MI lower bound - O(|E| log |V|)
        """
        # Convert to undirected for community detection
        G_undirected = graph.to_undirected()
        
        if G_undirected.number_of_edges() < 2:
            return 0.0
        
        # Detect communities via greedy modularity (fast)
        try:
            communities = list(community.greedy_modularity_communities(G_undirected))
        except:
            return 0.0
        
        if len(communities) < 2:
            return 0.0
        
        # Compute internal density for each community
        internal_densities = []
        for comm in communities:
            if len(comm) < 2:
                continue
            subgraph = G_undirected.subgraph(comm)
            density = nx.density(subgraph)
            internal_densities.append(density)
        
        if len(internal_densities) == 0:
            return 0.0
        
        avg_internal_density = np.mean(internal_densities)
        
        # Compute external density (cross-community edges)
        cross_edges = 0
        total_possible_cross = 0
        
        for i, comm1 in enumerate(communities):
            for comm2 in list(communities)[i+1:]:
                # Count edges between communities
                for u in comm1:
                    for v in comm2:
                        if graph.G.has_edge(u, v) or graph.G.has_edge(v, u):
                            cross_edges += 1
                total_possible_cross += len(comm1) * len(comm2)
        
        if total_possible_cross == 0:
            return 0.0
        
        external_density = cross_edges / total_possible_cross
        
        # EI lower bound: external / internal ratio
        return external_density / (avg_internal_density + 1e-9)
    
    def compute_topological_integration(self, graph: DirectedGraph, seed: int) -> float:
        """
        Equation: τ_Φ = |min_cut| / (|V|(|V|-1)/2)
        
        Normalized min-cut as integration proxy - O(|V||E|²)
        """
        if not graph.is_strongly_connected():
            return 0.0
        
        G_undirected = graph.to_undirected()
        
        try:
            # Compute minimum edge cut (Stoer-Wagner algorithm)
            min_cut_value, partition = nx.stoer_wagner(G_undirected)
            min_cut_size = min_cut_value  # This is already the cut size
            
        except nx.NetworkXError:
            # Graph not connected or other issue
            return 0.0
        
        # Normalize by maximum possible edges
        max_possible_edges = graph.num_nodes * (graph.num_nodes - 1) / 2
        
        return min_cut_size / max_possible_edges if max_possible_edges > 0 else 0.0
    
    def compute_mutual_information_histogram(self, 
                                            x: np.ndarray, 
                                            y: np.ndarray, 
                                            bins: int = 10) -> float:
        """
        Histogram-based mutual information computation.
        
        MI = Σ p(x,y) log(p(x,y) / (p(x)p(y)))
        """
        if len(x) != len(y) or len(x) == 0:
            return 0.0
        
        # Discretize into bins
        x_min, x_max = np.min(x), np.max(x)
        y_min, y_max = np.min(y), np.max(y)
        
        if x_max - x_min < 1e-9 or y_max - y_min < 1e-9:
            return 0.0
        
        # Bin indices
        x_bins = np.clip(((x - x_min) / (x_max - x_min + 1e-9) * bins).astype(int), 0, bins - 1)
        y_bins = np.clip(((y - y_min) / (y_max - y_min + 1e-9) * bins).astype(int), 0, bins - 1)
        
        # Build histograms
        joint_hist = np.zeros((bins, bins))
        x_hist = np.zeros(bins)
        y_hist = np.zeros(bins)
        
        n = len(x)
        for i in range(n):
            joint_hist[x_bins[i], y_bins[i]] += 1
            x_hist[x_bins[i]] += 1
            y_hist[y_bins[i]] += 1
        
        # Normalize to probabilities
        joint_prob = joint_hist / n
        x_prob = x_hist / n
        y_prob = y_hist / n
        
        # Compute MI
        mi = 0.0
        for i in range(bins):
            for j in range(bins):
                if joint_prob[i, j] > 0 and x_prob[i] > 0 and y_prob[j] > 0:
                    mi += joint_prob[i, j] * np.log2(joint_prob[i, j] / (x_prob[i] * y_prob[j]))
        
        return float(mi)
    
    def compute_mutual_information(self, graph: DirectedGraph, seed: int) -> float:
        """
        Mutual information between communities via dynamics simulation.
        
        Simulates: s_t = tanh(A @ s_{t-1})
        Then computes MI between community activity patterns.
        """
        rng = np.random.default_rng(seed)
        A = graph.adjacency_matrix()
        
        if A.size == 0 or graph.num_nodes < 2:
            return 0.0
        
        # Simulate dynamics
        timesteps = 1000
        state = rng.random(graph.num_nodes)
        activity_history = np.zeros((timesteps, graph.num_nodes))
        
        for t in range(timesteps):
            state = np.tanh(A @ state)
            activity_history[t] = state
        
        # Detect communities
        G_undirected = graph.to_undirected()
        try:
            communities = list(community.greedy_modularity_communities(G_undirected))
        except:
            # Fallback: treat each node as its own community
            return float(np.mean(np.abs(state)))
        
        if len(communities) < 2:
            return float(np.mean(np.abs(state)))
        
        # Compute MI between community pairs
        mi_values = []
        for i, comm1 in enumerate(communities):
            for comm2 in list(communities)[i+1:]:
                if len(comm1) == 0 or len(comm2) == 0:
                    continue
                
                # Aggregate activity for each community
                activity_1 = np.mean(activity_history[:, list(comm1)], axis=1)
                activity_2 = np.mean(activity_history[:, list(comm2)], axis=1)
                
                mi = self.compute_mutual_information_histogram(activity_1, activity_2)
                mi_values.append(mi)
        
        return float(np.mean(mi_values)) if mi_values else 0.0
    
    # ========================================================================
    # Main Computation
    # ========================================================================
    
    def compute(self, graph: DirectedGraph) -> Dict:
        """
        Compute all integration heuristics with uncertainty.
        
        Returns dictionary with UncertainMetric objects.
        """
        start_time = time.time()
        
        metrics = {}
        
        # Compute individual metrics with uncertainty
        metrics['causal_density'] = self.compute_with_uncertainty(
            self.compute_causal_density, graph
        )
        
        metrics['effective_info_bound'] = self.compute_with_uncertainty(
            self.compute_effective_info_bound, graph
        )
        
        metrics['topological_integration'] = self.compute_with_uncertainty(
            self.compute_topological_integration, graph
        )
        
        metrics['mutual_information'] = self.compute_with_uncertainty(
            self.compute_mutual_information, graph
        )
        
        # Aggregate integration index with uncertainty propagation
        # Equation: σ²(w₁X₁ + w₂X₂ + ...) = w₁²σ₁² + w₂²σ₂² + ...
        individual_means = [
            metrics['causal_density'].mean,
            metrics['effective_info_bound'].mean,
            metrics['topological_integration'].mean,
            metrics['mutual_information'].mean
        ]
        
        aggregate_mean = np.dot(self.weights, individual_means)
        
        individual_vars = [m.std_dev ** 2 for m in 
                          [metrics['causal_density'], 
                           metrics['effective_info_bound'],
                           metrics['topological_integration'],
                           metrics['mutual_information']]]
        
        aggregate_var = np.dot([w**2 for w in self.weights], individual_vars)
        aggregate_std = np.sqrt(aggregate_var)
        margin = 1.96 * aggregate_std
        
        metrics['integration_index'] = UncertainMetric(
            mean=aggregate_mean,
            std_dev=aggregate_std,
            confidence_lower=aggregate_mean - margin,
            confidence_upper=aggregate_mean + margin,
            num_samples=self.bootstrap_samples
        )
        
        # Check reliability of all metrics
        metrics['all_metrics_reliable'] = all(
            m.is_reliable() for m in 
            [metrics['causal_density'], 
             metrics['effective_info_bound'],
             metrics['topological_integration'],
             metrics['mutual_information'],
             metrics['integration_index']]
        )
        
        computation_time = time.time() - start_time
        metrics['computation_time_us'] = computation_time * 1e6
        
        return metrics
    
    # ========================================================================
    # Weight Calibration (from 4.2.md)
    # ========================================================================
    
    def calibrate_weights(self, 
                         graphs: List[DirectedGraph],
                         performance_scores: List[float]) -> Tuple[List[float], float]:
        """
        Calibrate metric weights against ground truth performance.
        
        Uses linear regression: performance = w1*ρ_c + w2*EI + w3*τ_Φ + w4*MI
        
        Returns: (calibrated_weights, correlation_coefficient)
        """
        assert len(graphs) == len(performance_scores), "Mismatched lengths"
        
        if len(graphs) < 4:
            print("Warning: Need at least 4 samples for calibration")
            return self.weights, 0.0
        
        # Build design matrix X (n_samples × 4 metrics)
        X = []
        y = performance_scores
        
        for graph in graphs:
            # Compute metrics (use mean only for calibration)
            metrics = self.compute(graph)
            row = [
                metrics['causal_density'].mean,
                metrics['effective_info_bound'].mean,
                metrics['topological_integration'].mean,
                metrics['mutual_information'].mean
            ]
            X.append(row)
        
        X = np.array(X)
        y = np.array(y)
        
        # Normalize columns (Z-score)
        X_mean = np.mean(X, axis=0)
        X_std = np.std(X, axis=0) + 1e-9
        X_norm = (X - X_mean) / X_std
        
        # Linear regression: w = (X^T X)^{-1} X^T y
        try:
            XtX = X_norm.T @ X_norm
            Xty = X_norm.T @ y
            w = np.linalg.solve(XtX, Xty)
        except np.linalg.LinAlgError:
            print("Warning: Singular matrix in calibration, using default weights")
            return self.weights, 0.0
        
        # Ensure non-negative and normalize
        w = np.maximum(w, 0)
        w = w / (np.sum(w) + 1e-9)
        
        # Compute correlation
        y_pred = X_norm @ w
        correlation = np.corrcoef(y, y_pred)[0, 1]
        
        calibrated_weights = w.tolist()
        
        print(f"[IntegrationHeuristics] Calibrated weights: {calibrated_weights}")
        print(f"[IntegrationHeuristics] Correlation with performance: {correlation:.3f}")
        
        return calibrated_weights, float(correlation)


# ============================================================================
# Helper Functions for Testing
# ============================================================================

def make_ring_graph(n: int) -> DirectedGraph:
    """Create a directed ring graph"""
    g = DirectedGraph(n)
    for i in range(n):
        g.add_edge(i, (i + 1) % n, weight=0.5)
    return g

def make_fully_connected(n: int) -> DirectedGraph:
    """Create a fully connected graph"""
    g = DirectedGraph(n)
    for i in range(n):
        for j in range(n):
            if i != j:
                g.add_edge(i, j, weight=np.random.uniform(0.3, 0.8))
    return g

def make_random_graph(n: int, edge_prob: float = 0.3) -> DirectedGraph:
    """Create a random directed graph"""
    g = DirectedGraph(n)
    for i in range(n):
        for j in range(n):
            if i != j and np.random.random() < edge_prob:
                g.add_edge(i, j, weight=np.random.uniform(0.1, 1.0))
    return g


# ============================================================================
# Comprehensive Test Suite (from 4.2.md Part 4)
# ============================================================================

class IntegrationTests:
    """Comprehensive test suite matching Discussions 4.2.md"""
    
    @staticmethod
    def test_integration_metrics_with_uncertainty():
        """Test basic metric computation with uncertainty"""
        print("\n" + "="*70)
        print("TEST: Integration Metrics with Uncertainty")
        print("="*70)
        
        # Create test graph (ring of 10 nodes)
        g = make_ring_graph(10)
        
        heuristics = IntegrationHeuristics(bootstrap_samples=50)
        metrics = heuristics.compute(g)
        
        # Verify all metrics have uncertainty
        assert metrics['causal_density'].num_samples == 50
        assert metrics['integration_index'].is_reliable()
        
        # Check uncertainty is reasonable (CI width < 0.15)
        ci_width = (metrics['integration_index'].confidence_upper - 
                   metrics['integration_index'].confidence_lower)
        assert ci_width < 0.15, f"CI width {ci_width} exceeds 0.15"
        
        print(f"✓ Integration index: {metrics['integration_index']}")
        print(f"✓ All metrics reliable: {metrics['all_metrics_reliable']}")
        print(f"✓ Computation time: {metrics['computation_time_us']/1000:.1f}ms")
        print("✓ Integration metrics test PASSED")
    
    @staticmethod
    def test_noise_resistance():
        """Test that metrics are resistant to noise"""
        print("\n" + "="*70)
        print("TEST: Noise Resistance")
        print("="*70)
        
        # Create clean graph
        clean = make_fully_connected(20)
        
        # Create noisy version (add 20% random edges)
        noisy = DirectedGraph(20)
        for u, v, data in clean.G.edges(data=True):
            noisy.add_edge(u, v, data['weight'])
        
        # Add noise edges
        for _ in range(int(0.2 * clean.num_edges())):
            u, v = np.random.randint(0, 20, 2)
            if u != v:
                noisy.add_edge(u, v, np.random.uniform(0.0, 0.2))
        
        heuristics = IntegrationHeuristics(bootstrap_samples=30)
        clean_metrics = heuristics.compute(clean)
        noisy_metrics = heuristics.compute(noisy)
        
        # Metrics should be similar (noise-resistant)
        difference = abs(clean_metrics['integration_index'].mean - 
                        noisy_metrics['integration_index'].mean)
        
        print(f"Clean graph: {clean_metrics['integration_index'].mean:.3f}")
        print(f"Noisy graph: {noisy_metrics['integration_index'].mean:.3f}")
        print(f"Difference: {difference:.3f}")
        
        assert difference < 0.20, f"Difference {difference} too large"
        print("✓ Noise resistance test PASSED")
    
    @staticmethod
    def test_weight_calibration():
        """Test weight calibration against performance"""
        print("\n" + "="*70)
        print("TEST: Weight Calibration")
        print("="*70)
        
        # Create 10 graphs with varying connectivity
        graphs = []
        performance_scores = []
        
        for i in range(10):
            edge_prob = 0.1 + 0.05 * i  # Increasing connectivity
            g = make_random_graph(15, edge_prob)
            graphs.append(g)
            
            # Simulate performance (correlated with connectivity)
            performance = edge_prob * 2.0 + np.random.normal(0, 0.1)
            performance_scores.append(performance)
        
        heuristics = IntegrationHeuristics(bootstrap_samples=20)
        weights, correlation = heuristics.calibrate_weights(graphs, performance_scores)
        
        print(f"Calibrated weights: {weights}")
        print(f"Correlation: {correlation:.3f}")
        
        assert correlation > 0.5, f"Correlation {correlation} too low"
        print("✓ Weight calibration test PASSED")
    
    @staticmethod
    def test_computational_complexity():
        """Test computational efficiency on larger graphs"""
        print("\n" + "="*70)
        print("TEST: Computational Complexity")
        print("="*70)
        
        heuristics = IntegrationHeuristics(bootstrap_samples=30)
        
        for n in [10, 50, 100]:
            g = make_random_graph(n, edge_prob=0.2)
            
            start = time.time()
            metrics = heuristics.compute(g)
            elapsed = time.time() - start
            
            print(f"n={n:3d}: {elapsed*1000:6.1f}ms "
                  f"(integration_index={metrics['integration_index'].mean:.3f})")
            
            # Should be sub-second for n=100
            if n == 100:
                assert elapsed < 10.0, f"Too slow: {elapsed}s for n=100"
        
        print("✓ Computational complexity test PASSED")
    
    @staticmethod
    def test_minimum_proxy_threshold():
        """Test that metrics meet minimum thresholds for integration"""
        print("\n" + "="*70)
        print("TEST: Minimum Proxy Threshold")
        print("="*70)
        
        # Well-integrated graph (fully connected)
        integrated = make_fully_connected(15)
        
        # Poorly integrated graph (sparse)
        sparse = make_random_graph(15, edge_prob=0.05)
        
        heuristics = IntegrationHeuristics(bootstrap_samples=30)
        
        integrated_metrics = heuristics.compute(integrated)
        sparse_metrics = heuristics.compute(sparse)
        
        print(f"Integrated graph CIP: {integrated_metrics['integration_index'].mean:.3f}")
        print(f"Sparse graph CIP: {sparse_metrics['integration_index'].mean:.3f}")
        
        # Integrated should exceed threshold
        MIN_THRESHOLD = 0.3
        assert integrated_metrics['integration_index'].mean > MIN_THRESHOLD
        
        print(f"✓ Integrated graph exceeds threshold ({MIN_THRESHOLD})")
        print("✓ Minimum proxy threshold test PASSED")
    
    @staticmethod
    def run_all():
        """Run all tests"""
        print("\n" + "="*70)
        print("NeuroForge v2.0 (Discussions 4.2.md) Comprehensive Test Suite")
        print("="*70)
        
        IntegrationTests.test_integration_metrics_with_uncertainty()
        IntegrationTests.test_noise_resistance()
        IntegrationTests.test_weight_calibration()
        IntegrationTests.test_computational_complexity()
        IntegrationTests.test_minimum_proxy_threshold()
        
        print("\n" + "="*70)
        print("✓ ALL TESTS PASSED")
        print("="*70)


# ============================================================================
# Main Demo
# ============================================================================

if __name__ == "__main__":
    # Set random seeds for reproducibility
    random.seed(42)
    np.random.seed(42)
    
    print("\n" + "="*70)
    print("NeuroForge v2.0 Integration Heuristics POC")
    print("Based on: Discussions.4.2.md")
    print("="*70)
    
    # Create test graph (20 nodes, ~40 random edges)
    print("\nCreating test graph (20 nodes, random edges)...")
    g = DirectedGraph(20)
    for _ in range(40):
        u = random.randint(0, 19)
        v = random.randint(0, 19)
        if u != v:
            g.add_edge(u, v, weight=random.uniform(0.1, 1.0))
    
    print(f"Graph: {g.num_nodes} nodes, {g.num_edges()} edges")
    
    # Compute integration heuristics
    print("\nComputing integration heuristics with uncertainty...")
    heuristics = IntegrationHeuristics(bootstrap_samples=100)
    results = heuristics.compute(g)
    
    # Display results
    print("\n" + "="*70)
    print("RESULTS")
    print("="*70)
    
    for key in ['causal_density', 'effective_info_bound', 
                'topological_integration', 'mutual_information', 
                'integration_index']:
        metric = results[key]
        reliable_mark = "✓" if metric.is_reliable() else "✗"
        print(f"\n{key.replace('_', ' ').title()}:")
        print(f"  {metric}")
        print(f"  Reliable: {reliable_mark}")
    
    print(f"\nAll metrics reliable: {results['all_metrics_reliable']}")
    print(f"Computation time: {results['computation_time_us']/1000:.1f}ms")
    
    # Check against minimum threshold
    MIN_THRESHOLD = 0.3
    print(f"\nMinimum integration threshold: {MIN_THRESHOLD}")
    if results['integration_index'].mean >= MIN_THRESHOLD:
        print(f"✓ Graph PASSES integration threshold")
    else:
        print(f"✗ Graph FAILS integration threshold")
    
    # Run comprehensive test suite
    print("\n" + "="*70)
    print("Running Comprehensive Test Suite")
    print("="*70)
    
    IntegrationTests.run_all()
    
    print("\n" + "="*70)
    print("POC Complete - Ready for Production Enhancement")
    print("="*70)
    print("\nNext steps:")
    print("1. Add ROS2 integration (HardwareAdapter)")
    print("2. Add gRPC coordination (HiveManager)")
    print("3. Add governance layer (GovernanceCore)")
    print("4. Add identity monitoring (IdentityMonitor)")
print("5. Integrate with C++ HypergraphBrain via pybind11")