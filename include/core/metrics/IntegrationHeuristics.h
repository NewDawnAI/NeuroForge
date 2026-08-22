#pragma once

#include <vector>
#include <array>
#include <string>
#include <functional>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <numeric>
#include <format>
#include <random>
#include <Eigen/Dense>

namespace NeuroForge {
namespace Metrics {

/**
 * @brief Directed Graph representation for integration analysis
 */
struct DirectedGraph {
    Eigen::MatrixXf adjacency_matrix; // Weight from j to i
    int num_nodes;

    DirectedGraph(int n) : num_nodes(n) {
        adjacency_matrix = Eigen::MatrixXf::Zero(n, n);
    }
};

struct UncertainMetric {
    float mean;
    float std_dev;
    float confidence_lower;  // 95% CI
    float confidence_upper;
    int num_samples;
    
    // Convert to string for logging
    std::string toString() const {
        // Simple string formatting if std::format is not available or strictly compliant
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%.3f +/- %.3f (CI: [%.3f, %.3f], n=%d)",
                 mean, std_dev, confidence_lower, confidence_upper, num_samples);
        return std::string(buffer);
    }
};

/**
 * @brief Integration heuristics (NOT true IIT Phi)
 * 
 * These are computable graph statistics that CORRELATE with
 * integration, but make no claim to measure consciousness.
 */
class IntegrationHeuristics {
public:
    struct Metrics {
        UncertainMetric causal_density;
        UncertainMetric effective_info_bound;
        UncertainMetric topological_integration;
        UncertainMetric mutual_information;
        
        // Aggregate with learned weights + uncertainty propagation
        UncertainMetric integration_index;
        
        // Metadata
        uint64_t computation_time_us;
        bool all_metrics_reliable;
    };
    
    struct Config {
        int bootstrap_samples = 50;          // For uncertainty estimation
        std::array<float, 4> weights = {0.25f, 0.25f, 0.25f, 0.25f};  // Learned from data
        bool enable_adaptive_weights = true;  // Auto-tune via performance feedback
    };
    
    explicit IntegrationHeuristics(const Config& cfg) : config_(cfg) {}
    
    /**
     * @brief Compute integration heuristics with uncertainty
     * @return Metrics with confidence intervals
     */
    Metrics compute(const DirectedGraph& graph);
    
    /**
     * @brief Calibrate weights against ground truth performance
     * @param graphs Training graphs
     * @param performance_scores Known performance on tasks (e.g., maze completion)
     * @return Calibrated weights and correlation coefficient
     */
    std::pair<std::array<float, 4>, float> calibrateWeights(
        const std::vector<DirectedGraph>& graphs,
        const std::vector<float>& performance_scores
    );
    
private:
    Config config_;
    
    // Bootstrap resampling for uncertainty
    UncertainMetric computeWithUncertainty(
        std::function<float(const DirectedGraph&, int seed)> metric_fn,
        const DirectedGraph& graph
    );
    
    // Individual metrics
    float computeCausalDensity(const DirectedGraph& g, int seed);
    float computeEffectiveInfoBound(const DirectedGraph& g, int seed);
    float computeTopologicalIntegration(const DirectedGraph& g, int seed);
    float computeMutualInformation(const DirectedGraph& g, int seed);

    // Helpers for Mutual Information
    std::vector<std::vector<int>> detectCommunities(const DirectedGraph& g, std::mt19937& rng);
    Eigen::MatrixXf simulateDynamics(const DirectedGraph& g, int timesteps, std::mt19937& rng);
    Eigen::VectorXf extractCommunityActivity(const Eigen::MatrixXf& activity, const std::vector<int>& community_nodes);
    float computeMutualInformationHistogram(const Eigen::VectorXf& x, const Eigen::VectorXf& y);
};

} // namespace Metrics
} // namespace NeuroForge
