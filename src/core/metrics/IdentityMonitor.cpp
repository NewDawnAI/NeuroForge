#include "core/metrics/IdentityMonitor.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace NeuroForge {
namespace Identity {

IdentityMonitor::IdentityMonitor(const Config& cfg) : config_(cfg) {
    mean_ = Eigen::Vector4f::Zero();
    covariance_ = Eigen::Matrix4f::Identity();
    precision_ = Eigen::Matrix4f::Identity();
}

std::pair<float, float> IdentityMonitor::updateAndCheckDrift(const SelfState& current) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Update normalization bounds
    updateNormalizationBounds(current);
    
    if (history_.empty()) {
        history_.push_back(current);
        updateCovariance();
        return {-1.0f, 0.0f};  // Insufficient history
    }
    
    // Compute drift
    float drift;
    if (covariance_ready_) {
        // Use Mahalanobis distance
        drift = current.distanceTo(history_.back(), precision_);
    } else {
        // Fallback to Euclidean until covariance is learned
        Eigen::Vector4f diff = current.toVector() - history_.back().toVector();
        drift = diff.norm();
    }
    
    drift_history_.push_back(drift);
    history_.push_back(current);
    
    // Trim history
    if (history_.size() > config_.history_size) {
        history_.pop_front();
    }
    if (drift_history_.size() > config_.history_size) {
        drift_history_.pop_front();
    }
    
    // Update covariance periodically
    if (history_.size() % 10 == 0) {
        updateCovariance();
    }
    
    // Compute probability of excessive drift
    // Assume drift follows chi-squared distribution (Mahalanobis property)
    // For 4 DOF, critical value at 95% is ~9.49
    float critical_value = 9.49f;
    float p_excessive = (drift > critical_value) ? 1.0f : (drift / critical_value);
    
    return {drift, p_excessive};
}

void IdentityMonitor::updateCovariance() {
    if (history_.size() < config_.covariance_warmup_samples) {
        return;
    }
    
    // Compute empirical mean
    mean_ = Eigen::Vector4f::Zero();
    for (const auto& state : history_) {
        mean_ += state.toVector();
    }
    mean_ /= static_cast<float>(history_.size());
    
    // Compute empirical covariance
    covariance_ = Eigen::Matrix4f::Zero();
    for (const auto& state : history_) {
        Eigen::Vector4f centered = state.toVector() - mean_;
        covariance_ += centered * centered.transpose();
    }
    covariance_ /= static_cast<float>(history_.size() - 1);
    
    // Add regularization to prevent singularity
    covariance_ += 0.01f * Eigen::Matrix4f::Identity();
    
    // Compute precision (inverse)
    Eigen::LDLT<Eigen::Matrix4f> ldlt(covariance_);
    if (ldlt.info() == Eigen::Success) {
        precision_ = ldlt.solve(Eigen::Matrix4f::Identity());
        covariance_ready_ = true;
    }
}

void IdentityMonitor::updateNormalizationBounds(const SelfState& state) {
    // Use exponential moving average for bounds
    const float alpha = 0.01f;  // Slow adaptation
    
    // Update max for prediction error (denormalize first if needed)
    float current_error_unnorm = state.prediction_error_norm * prediction_error_max_;
    prediction_error_max_ = std::max(
        prediction_error_max_,
        alpha * current_error_unnorm + (1 - alpha) * prediction_error_max_
    );
    
    // Similar for stability
    float current_stability_unnorm = state.stability_index_norm * stability_max_;
    stability_max_ = std::max(
        stability_max_,
        alpha * current_stability_unnorm + (1 - alpha) * stability_max_
    );
}

bool IdentityMonitor::isDriftCritical() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (drift_history_.empty()) return false;
    
    float recent_drift = drift_history_.back();
    
    // Critical if Mahalanobis distance > chi-squared critical value
    float critical_value = 9.49f;  // 95% for 4 DOF
    
    return recent_drift > critical_value;
}

IdentityMonitor::NormStats IdentityMonitor::getNormalizationStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {prediction_error_max_, stability_max_, history_.size() > 10};
}

} // namespace Identity
} // namespace NeuroForge
