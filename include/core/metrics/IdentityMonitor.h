#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <utility>
#include <Eigen/Dense>

namespace NeuroForge {
namespace Identity {

/**
 * @brief Self-state with proper normalization
 */
class IdentityMonitor {
public:
    struct SelfState {
        // All components normalized to [0,1]
        float integration_index_norm;    // From IntegrationHeuristics
        float prediction_error_norm;     // Normalized by moving max
        float goal_completion_rate;      // Already in [0,1]
        float stability_index_norm;      // Normalized change rate
        
        uint64_t timestamp_ms;
        
        Eigen::Vector4f toVector() const {
            return Eigen::Vector4f(
                integration_index_norm,
                prediction_error_norm,
                goal_completion_rate,
                stability_index_norm
            );
        }
        
        /**
         * @brief Mahalanobis distance with learned covariance
         * 
         * Distance accounts for correlations between components.
         */
        float distanceTo(
            const SelfState& other,
            const Eigen::Matrix4f& precision_matrix  // Inverse covariance
        ) const {
            Eigen::Vector4f diff = toVector() - other.toVector();
            return std::sqrt(std::abs(diff.transpose() * precision_matrix * diff));
        }
    };
    
    struct Config {
        uint64_t snapshot_interval_ms = 5000;
        float max_drift_probability = 0.05f;  // P(drift > threshold) < 5%
        size_t history_size = 100;
        size_t covariance_warmup_samples = 50;  // Before using Mahalanobis
    };
    
    explicit IdentityMonitor(const Config& cfg);
    
    /**
     * @brief Update with normalized state
     * @return {drift, probability_excessive}
     */
    std::pair<float, float> updateAndCheckDrift(const SelfState& current);
    
    /**
     * @brief Check if drift is statistically significant
     */
    bool isDriftCritical() const;
    
    /**
     * @brief Get normalization statistics for proper scaling
     */
    struct NormStats {
        float prediction_error_max;
        float stability_max;
        bool ready;  // Have enough samples
    };
    
    NormStats getNormalizationStats() const;
    
private:
    Config config_;
    std::deque<SelfState> history_;
    std::deque<float> drift_history_;
    
    // Learned statistics
    Eigen::Vector4f mean_;
    Eigen::Matrix4f covariance_;
    Eigen::Matrix4f precision_;  // Inverse covariance
    bool covariance_ready_ = false;
    
    // Normalization bounds (moving statistics)
    float prediction_error_max_ = 1.0f;
    float stability_max_ = 1.0f;
    
    mutable std::mutex mutex_;
    
    void updateCovariance();
    void updateNormalizationBounds(const SelfState& state);
};

} // namespace Identity
} // namespace NeuroForge
