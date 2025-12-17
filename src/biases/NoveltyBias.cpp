#include "biases/NoveltyBias.h"
#include <chrono>
#include <random>
#include <cstring>

namespace NeuroForge {
namespace Biases {

NoveltyBias::NoveltyBias(const Config& config) : config_(config) {
    // Initialize time tracking
    auto now = std::chrono::steady_clock::now();
    last_update_time_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

NoveltyBias::NoveltyMetrics NoveltyBias::calculateNovelty(const std::vector<float>& input) {
    if (!validateInput(input)) {
        return NoveltyMetrics{};
    }
    
    NoveltyMetrics metrics;
    
    // Use a single lock for the entire calculation to avoid deadlocks
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    // Initialize prediction model if needed
    if (prediction_model_.empty() && !input.empty()) {
        initializePredictionModel(input.size());
    }
    
    // Calculate familiarity score
    metrics.familiarity_score = calculateFamiliarityScoreUnsafe(input);
    
    // Calculate complexity score (no mutex needed)
    metrics.complexity_score = calculateComplexityScore(input);
    
    // Calculate surprise level
    metrics.surprise_level = calculateSurpriseLevelUnsafe(input);
    
    // Calculate information gain
    metrics.information_gain = calculateInformationGainUnsafe(input);
    
    // Calculate prediction error if we have a prediction model
    if (!prediction_model_.empty() && prediction_model_.size() == input.size()) {
        auto prediction = getPredictionUnsafe(input);
        metrics.prediction_error = calculatePredictionError(prediction, input);
    }
    
    // Compute exploration bonus
    metrics.exploration_bonus = computeExplorationBonus(metrics);
    
    // Update statistics
    updateStatisticsUnsafe(metrics);
    
    // Update experience buffer
    updateExperienceBufferUnsafe(input);
    
    total_experiences_.fetch_add(1);
    
    return metrics;
}

void NoveltyBias::updateExperienceBufferUnsafe(const std::vector<float>& experience) {
    if (!validateInput(experience)) {
        return;
    }
    
    // Add new experience
    experience_buffer_.push_back(experience);
    
    // Maintain buffer size limit
    while (experience_buffer_.size() > config_.experience_buffer_size) {
        experience_buffer_.pop_front();
    }
}

void NoveltyBias::updateExperienceBuffer(const std::vector<float>& experience) {
    if (!validateInput(experience)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    updateExperienceBufferUnsafe(experience);
}

float NoveltyBias::computeExplorationBonus(const NoveltyMetrics& metrics) {
    if (!config_.enable_exploration_bonus) {
        return 0.0f;
    }
    
    // Combine different novelty aspects
    float novelty_score = 0.0f;
    
    // Weight by prediction error (high error = more novel)
    novelty_score += metrics.prediction_error * 0.3f;
    
    // Weight by information gain (high gain = more valuable to explore)
    novelty_score += metrics.information_gain * 0.25f;
    
    // Weight by surprise level (high surprise = more interesting)
    novelty_score += metrics.surprise_level * 0.2f;
    
    // Weight by complexity (moderate complexity preferred)
    float complexity_bonus = 1.0f - std::abs(metrics.complexity_score - 0.5f) * 2.0f;
    novelty_score += complexity_bonus * config_.complexity_weight * 0.15f;
    
    // Inverse weight by familiarity (less familiar = more novel)
    novelty_score += (1.0f - metrics.familiarity_score) * 0.1f;
    
    // Apply scaling factor
    float exploration_bonus = novelty_score * config_.exploration_bonus_scale;
    
    // Clamp to reasonable range
    return std::clamp(exploration_bonus, 0.0f, 2.0f);
}

bool NoveltyBias::isNovel(const std::vector<float>& input, float threshold) {
    if (threshold < 0.0f) {
        threshold = config_.novelty_threshold;
    }
    
    auto metrics = calculateNovelty(input);
    
    // Consider novel if any metric exceeds threshold
    return (metrics.prediction_error > threshold ||
            metrics.information_gain > threshold ||
            metrics.surprise_level > threshold ||
            (1.0f - metrics.familiarity_score) > threshold);
}

void NoveltyBias::updatePredictionModel(const std::vector<float>& input,
                                       const std::vector<float>& actual_outcome) {
    if (!config_.enable_prediction_learning || !validateInput(input) || !validateInput(actual_outcome)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    // Initialize if needed
    if (prediction_model_.empty()) {
        initializePredictionModel(input.size());
    }
    
    // Ensure sizes match
    if (prediction_model_.size() != input.size() || input.size() != actual_outcome.size()) {
        return;
    }
    
    // Update prediction model using exponential moving average
    float learning_rate = config_.prediction_learning_rate;
    
    for (size_t i = 0; i < prediction_model_.size(); ++i) {
        // Update mean prediction
        prediction_model_[i] = (1.0f - learning_rate) * prediction_model_[i] + 
                              learning_rate * actual_outcome[i];
        
        // Update variance estimate
        float error = actual_outcome[i] - prediction_model_[i];
        prediction_variance_[i] = (1.0f - learning_rate) * prediction_variance_[i] + 
                                 learning_rate * error * error;
    }
}

std::vector<float> NoveltyBias::getPredictionUnsafe(const std::vector<float>& input) const {
    if (prediction_model_.empty() || prediction_model_.size() != input.size()) {
        return input; // Return input as prediction if no model
    }
    
    // Simple prediction: return learned average (could be enhanced with more sophisticated models)
    return prediction_model_;
}

std::vector<float> NoveltyBias::getPrediction(const std::vector<float>& input) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return getPredictionUnsafe(input);
}

float NoveltyBias::calculatePredictionError(const std::vector<float>& predicted,
                                          const std::vector<float>& actual) const {
    if (predicted.size() != actual.size() || predicted.empty()) {
        return 0.0f;
    }
    
    // Calculate mean squared error
    float mse = 0.0f;
    for (size_t i = 0; i < predicted.size(); ++i) {
        float error = predicted[i] - actual[i];
        mse += error * error;
    }
    mse /= predicted.size();
    
    // Normalize to [0,1] range (assuming input values are roughly [-1,1])
    return std::clamp(std::sqrt(mse), 0.0f, 1.0f);
}

float NoveltyBias::calculateInformationGainUnsafe(const std::vector<float>& input) const {
    if (input.empty()) {
        return 1.0f; // Maximum information gain for first experience
    }
    
    if (experience_buffer_.empty()) {
        return 1.0f; // Maximum information gain for first experience
    }
    
    // Calculate information gain based on how different this input is from existing experiences
    float min_similarity = 1.0f;
    
    for (const auto& experience : experience_buffer_) {
        if (experience.size() == input.size()) {
            float similarity = calculateCosineSimilarity(input, experience);
            min_similarity = std::min(min_similarity, similarity);
        }
    }
    
    // Information gain is inverse of maximum similarity
    float information_gain = 1.0f - std::max(min_similarity, 0.0f);
    
    return std::clamp(information_gain, 0.0f, 1.0f);
}

float NoveltyBias::calculateInformationGain(const std::vector<float>& input) const {
    if (input.empty()) {
        return 1.0f;
    }
    
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return calculateInformationGainUnsafe(input);
}

float NoveltyBias::calculateSurpriseLevelUnsafe(const std::vector<float>& input) const {
    if (input.empty()) {
        return 0.0f;
    }
    
    if (prediction_model_.empty() || prediction_variance_.empty()) {
        return 0.0f;
    }
    
    // Calculate surprise based on how far input deviates from expected variance
    float surprise = 0.0f;
    size_t valid_dims = 0;
    
    for (size_t i = 0; i < std::min(input.size(), prediction_model_.size()); ++i) {
        if (prediction_variance_[i] > 1e-10f && std::isfinite(input[i]) && std::isfinite(prediction_model_[i])) {
            float deviation = std::abs(input[i] - prediction_model_[i]);
            float normalized_deviation = deviation / std::sqrt(prediction_variance_[i]);
            if (std::isfinite(normalized_deviation)) {
                surprise += normalized_deviation;
                valid_dims++;
            }
        }
    }
    
    if (valid_dims > 0) {
        surprise /= valid_dims;
        surprise = std::tanh(surprise / config_.surprise_sensitivity); // Normalize with sensitivity
    }
    
    return std::clamp(surprise, 0.0f, 1.0f);
}

float NoveltyBias::calculateSurpriseLevel(const std::vector<float>& input) const {
    if (input.empty()) {
        return 0.0f;
    }
    
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return calculateSurpriseLevelUnsafe(input);
}

float NoveltyBias::calculateFamiliarityScoreUnsafe(const std::vector<float>& input) const {
    if (input.empty()) {
        return 0.0f;
    }
    
    if (experience_buffer_.empty()) {
        return 0.0f; // No familiarity with empty buffer
    }
    
    // Find maximum similarity with any experience
    float max_similarity = 0.0f;
    
    for (const auto& experience : experience_buffer_) {
        if (experience.size() == input.size()) {
            float similarity = calculateCosineSimilarity(input, experience);
            max_similarity = std::max(max_similarity, similarity);
        }
    }
    
    return std::clamp(max_similarity, 0.0f, 1.0f);
}

float NoveltyBias::calculateFamiliarityScore(const std::vector<float>& input) const {
    if (input.empty()) {
        return 0.0f;
    }
    
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return calculateFamiliarityScoreUnsafe(input);
}

float NoveltyBias::calculateComplexityScore(const std::vector<float>& input) const {
    if (input.empty()) {
        return 0.0f;
    }
    
    // Calculate complexity based on entropy and variance
    float entropy = calculateEntropy(input);
    
    // Calculate variance
    float mean = std::accumulate(input.begin(), input.end(), 0.0f) / input.size();
    float variance = 0.0f;
    for (float value : input) {
        variance += (value - mean) * (value - mean);
    }
    variance /= input.size();
    
    // Combine entropy and variance for complexity score
    float complexity = 0.7f * entropy + 0.3f * std::min(variance, 1.0f);
    
    return std::clamp(complexity, 0.0f, 1.0f);
}

void NoveltyBias::applyTemporalDecay(std::uint64_t delta_time_ms) {
    if (delta_time_ms == 0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    float decay_factor = 1.0f - (config_.familiarity_decay_rate * delta_time_ms / 1000.0f);
    decay_factor = std::max(decay_factor, 0.0f);
    
    // Apply decay to prediction model confidence (increase variance)
    for (float& variance : prediction_variance_) {
        variance *= (1.0f + config_.familiarity_decay_rate * delta_time_ms / 1000.0f);
    }
}

void NoveltyBias::updateTime(std::uint64_t current_time_ms) {
    if (current_time_ms > last_update_time_ms_) {
        applyTemporalDecay(current_time_ms - last_update_time_ms_);
        last_update_time_ms_ = current_time_ms;
    }
}

NoveltyBias::Statistics NoveltyBias::getStatistics() const {
    Statistics stats;
    
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    stats.total_experiences = total_experiences_.load();
    stats.novel_experiences = novel_experiences_.load();
    stats.familiar_experiences = familiar_experiences_.load();
    
    if (stats.total_experiences > 0) {
        stats.novelty_rate = static_cast<float>(stats.novel_experiences) / stats.total_experiences;
    }
    
    stats.average_novelty = average_novelty_.load();
    stats.average_exploration_bonus = average_exploration_bonus_.load();
    stats.experience_buffer_size = experience_buffer_.size();
    stats.prediction_model_size = prediction_model_.size();
    stats.prediction_learning_active = config_.enable_prediction_learning;
    stats.exploration_bonus_active = config_.enable_exploration_bonus;
    
    return stats;
}

void NoveltyBias::setNoveltyThreshold(float threshold) {
    config_.novelty_threshold = std::clamp(threshold, 0.0f, 1.0f);
}

float NoveltyBias::getNoveltyThreshold() const {
    return config_.novelty_threshold;
}

void NoveltyBias::setConfig(const Config& new_config) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    config_ = new_config;
    
    // Adjust buffer size if needed
    while (experience_buffer_.size() > config_.experience_buffer_size) {
        experience_buffer_.pop_front();
    }
}

NoveltyBias::Config NoveltyBias::getConfig() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return config_;
}

void NoveltyBias::clear() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    experience_buffer_.clear();
    prediction_model_.clear();
    prediction_variance_.clear();
    
    total_experiences_.store(0);
    novel_experiences_.store(0);
    familiar_experiences_.store(0);
    average_novelty_.store(0.0f);
    average_exploration_bonus_.store(0.0f);
}

bool NoveltyBias::isOperational() const {
    return true; // Always operational
}

void NoveltyBias::initializePredictionModel(size_t input_size) {
    prediction_model_.resize(input_size, 0.0f);
    prediction_variance_.resize(input_size, 1.0f); // Start with high variance
}

float NoveltyBias::calculateCosineSimilarity(const std::vector<float>& vec_a,
                                           const std::vector<float>& vec_b) const {
    if (vec_a.size() != vec_b.size() || vec_a.empty()) {
        return 0.0f;
    }
    
    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    
    for (size_t i = 0; i < vec_a.size(); ++i) {
        // Check for invalid values
        if (!std::isfinite(vec_a[i]) || !std::isfinite(vec_b[i])) {
            continue;
        }
        dot_product += vec_a[i] * vec_b[i];
        norm_a += vec_a[i] * vec_a[i];
        norm_b += vec_b[i] * vec_b[i];
    }
    
    if (norm_a <= 1e-10f || norm_b <= 1e-10f) {
        return 0.0f;
    }
    
    float result = dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
    return std::isfinite(result) ? std::clamp(result, -1.0f, 1.0f) : 0.0f;
}

float NoveltyBias::calculateEuclideanDistance(const std::vector<float>& vec_a,
                                            const std::vector<float>& vec_b) const {
    if (vec_a.size() != vec_b.size() || vec_a.empty()) {
        return 0.0f;
    }
    
    float distance = 0.0f;
    for (size_t i = 0; i < vec_a.size(); ++i) {
        float diff = vec_a[i] - vec_b[i];
        distance += diff * diff;
    }
    
    return std::sqrt(distance);
}

float NoveltyBias::calculateEntropy(const std::vector<float>& input) const {
    if (input.empty()) {
        return 0.0f;
    }
    
    // Simple entropy calculation based on value distribution
    // Discretize values into bins for entropy calculation
    const int num_bins = 10;
    std::vector<int> bins(num_bins, 0);
    
    // Find min/max for normalization
    auto minmax = std::minmax_element(input.begin(), input.end());
    float min_val = *minmax.first;
    float max_val = *minmax.second;
    
    if (max_val == min_val) {
        return 0.0f; // No entropy if all values are the same
    }
    
    // Bin the values
    for (float value : input) {
        int bin = static_cast<int>((value - min_val) / (max_val - min_val) * (num_bins - 1));
        bin = std::clamp(bin, 0, num_bins - 1);
        bins[bin]++;
    }
    
    // Calculate entropy
    float entropy = 0.0f;
    float total = static_cast<float>(input.size());
    
    for (int count : bins) {
        if (count > 0) {
            float probability = count / total;
            entropy -= probability * std::log2(probability);
        }
    }
    
    // Normalize by maximum possible entropy
    float max_entropy = std::log2(static_cast<float>(num_bins));
    return max_entropy > 0.0f ? entropy / max_entropy : 0.0f;
}

int NoveltyBias::findMostSimilarExperience(const std::vector<float>& input) const {
    if (experience_buffer_.empty() || input.empty()) {
        return -1;
    }
    
    int best_index = -1;
    float best_similarity = -1.0f;
    
    for (size_t i = 0; i < experience_buffer_.size(); ++i) {
        if (experience_buffer_[i].size() == input.size()) {
            float similarity = calculateCosineSimilarity(input, experience_buffer_[i]);
            if (similarity > best_similarity) {
                best_similarity = similarity;
                best_index = static_cast<int>(i);
            }
        }
    }
    
    return best_index;
}

void NoveltyBias::updateStatisticsUnsafe(const NoveltyMetrics& metrics) {
    // Update running averages
    float total = static_cast<float>(total_experiences_.load() + 1);
    
    float current_avg_novelty = average_novelty_.load();
    float new_novelty = (metrics.prediction_error + metrics.information_gain + 
                        metrics.surprise_level + (1.0f - metrics.familiarity_score)) / 4.0f;
    float updated_avg_novelty = (current_avg_novelty * (total - 1) + new_novelty) / total;
    average_novelty_.store(updated_avg_novelty);
    
    float current_avg_bonus = average_exploration_bonus_.load();
    float updated_avg_bonus = (current_avg_bonus * (total - 1) + metrics.exploration_bonus) / total;
    average_exploration_bonus_.store(updated_avg_bonus);
    
    // Update novel/familiar counts
    if (new_novelty > config_.novelty_threshold) {
        novel_experiences_.fetch_add(1);
    } else {
        familiar_experiences_.fetch_add(1);
    }
}

bool NoveltyBias::validateInput(const std::vector<float>& input) const {
    if (input.empty() || input.size() > 10000) { // Reasonable size limit
        return false;
    }
    
    // Check for invalid values
    for (float value : input) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    
    return true;
}

} // namespace Biases
} // namespace NeuroForge