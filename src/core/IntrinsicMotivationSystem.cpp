#include "core/IntrinsicMotivationSystem.h"
#include "core/HypergraphBrain.h"
#include "core/LearningSystem.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>

namespace NeuroForge {
namespace Core {

IntrinsicMotivationSystem::IntrinsicMotivationSystem(std::shared_ptr<HypergraphBrain> brain,
                                                   const Config& config)
    : brain_(brain)
    , config_(config)
    , last_update_time_(std::chrono::steady_clock::now()) {
    
    // Initialize novelty detector
    novelty_detector_.memory_capacity = config_.novelty_memory_size;
    novelty_detector_.novelty_threshold = 0.5f;
}

bool IntrinsicMotivationSystem::initialize() {
    if (!brain_) {
        return false;
    }
    
    is_active_.store(true);
    last_update_time_ = std::chrono::steady_clock::now();
    
    // Initialize prediction tracker
    prediction_tracker_.predictions.reserve(config_.prediction_window);
    prediction_tracker_.actual_outcomes.reserve(config_.prediction_window);
    prediction_tracker_.errors.reserve(config_.prediction_window);
    
    // Initialize novelty detector memory
    novelty_detector_.experience_memory.reserve(novelty_detector_.memory_capacity);
    
    return true;
}

void IntrinsicMotivationSystem::shutdown() {
    is_active_.store(false);
}

IntrinsicMotivationSystem::MotivationComponents IntrinsicMotivationSystem::updateMotivation(float delta_time) {
    if (!is_active_.load() || !brain_) {
        return current_motivation_;
    }
    
    std::lock_guard<std::mutex> lock(motivation_mutex_);
    
    // Apply decay to previous motivation
    applyMotivationDecay(delta_time);
    
    // Calculate new motivation components
    MotivationComponents new_motivation;
    new_motivation.timestamp = std::chrono::steady_clock::now();
    
    // Calculate uncertainty-based motivation
    new_motivation.uncertainty = calculateUncertaintyMotivation();
    
    // Calculate curiosity-driven motivation
    new_motivation.curiosity = calculateCuriosityMotivation();
    
    // Calculate competence-based motivation
    if (config_.enable_competence_motivation) {
        new_motivation.competence = calculateCompetenceMotivation();
    }
    
    // Calculate exploration motivation
    new_motivation.exploration = calculateExplorationMotivation();
    
    // Calculate meta-learning motivation
    if (config_.enable_meta_learning) {
        new_motivation.meta_learning = calculateMetaLearningMotivation();
    }
    
    // Get current substrate state for novelty calculation
    std::vector<float> current_state;
    const auto& regions = brain_->getRegionsMap();
    for (const auto& region_pair : regions) {
        if (region_pair.second) {
            current_state.push_back(region_pair.second->getGlobalActivation());
        }
    }
    
    // Calculate novelty motivation
    new_motivation.novelty = calculateNoveltyMotivation(current_state);
    
    // Normalize components
    normalizeMotivationComponents(new_motivation);
    
    // Calculate composite motivation
    new_motivation.composite = calculateCompositeMotivation(new_motivation);
    
    // Update current motivation
    current_motivation_ = new_motivation;
    
    // Update history
    updateMotivationHistory(new_motivation);
    
    return current_motivation_;
}

float IntrinsicMotivationSystem::calculateUncertaintyMotivation() {
    if (!brain_) return 0.0f;
    
    // Calculate uncertainty based on activation variance and prediction errors
    float activation_variance = calculateActivationVariance();
    float prediction_uncertainty = prediction_tracker_.error_variance;
    float learning_dynamics = calculateLearningRateDynamics();
    
    // Combine uncertainty sources
    float uncertainty = (activation_variance * 0.4f) + 
                       (prediction_uncertainty * 0.4f) + 
                       (learning_dynamics * 0.2f);
    
    return std::clamp(uncertainty, 0.0f, 1.0f);
}

float IntrinsicMotivationSystem::calculatePredictionErrorMotivation(float prediction, float actual_outcome) {
    // Update prediction tracker
    updatePredictionTracker(prediction, actual_outcome);
    
    // Calculate motivation based on prediction error magnitude
    float error = std::abs(prediction - actual_outcome);
    float normalized_error = std::tanh(error); // Normalize to [0, 1] range
    
    // Higher prediction errors generate higher motivation
    return normalized_error;
}

float IntrinsicMotivationSystem::calculateNoveltyMotivation(const std::vector<float>& experience) {
    if (experience.empty()) return 0.0f;
    
    // Update novelty detector and get novelty score
    float novelty_score = updateNoveltyDetector(experience);
    
    // Convert novelty score to motivation
    return std::clamp(novelty_score, 0.0f, 1.0f);
}

float IntrinsicMotivationSystem::calculateCuriosityMotivation() {
    if (!brain_) return 0.0f;
    
    // Calculate curiosity based on information gain potential
    float substrate_complexity = calculateSubstrateComplexity();
    float learning_potential = 1.0f - calculateCompetenceMotivation(); // Inverse of competence
    float exploration_need = calculateExplorationMotivation();
    
    // Combine curiosity factors
    float curiosity = (substrate_complexity * 0.4f) + 
                     (learning_potential * 0.4f) + 
                     (exploration_need * 0.2f);
    
    return std::clamp(curiosity, 0.0f, 1.0f);
}

float IntrinsicMotivationSystem::calculateCompetenceMotivation() {
    if (!brain_) return 0.0f;
    
    auto* learning_system = brain_->getLearningSystem();
    if (!learning_system) return 0.0f;
    
    // Get competence level from learning system
    float competence_level = learning_system->getCompetenceLevel();
    
    // Motivation to improve competence (higher when competence is moderate)
    // Peak motivation at 50% competence, lower at extremes
    float competence_motivation = 4.0f * competence_level * (1.0f - competence_level);
    
    return std::clamp(competence_motivation, 0.0f, 1.0f);
}

float IntrinsicMotivationSystem::calculateExplorationMotivation() {
    if (!brain_) return 0.0f;
    
    // Calculate exploration motivation based on:
    // 1. Uncertainty in current state
    // 2. Novelty potential
    // 3. Learning progress stagnation
    
    float uncertainty = calculateUncertaintyMotivation();
    float novelty_potential = 1.0f - (novelty_detector_.novelty_scores.empty() ? 
        0.0f : *std::max_element(novelty_detector_.novelty_scores.begin(), 
                                novelty_detector_.novelty_scores.end()));
    
    // Check for learning stagnation
    float learning_stagnation = 0.0f;
    if (motivation_history_.size() >= 5) {
        // Calculate variance in recent competence scores
        std::vector<float> recent_competence;
        for (std::size_t i = motivation_history_.size() - 5; i < motivation_history_.size(); ++i) {
            recent_competence.push_back(motivation_history_[i].competence);
        }
        
        float mean = std::accumulate(recent_competence.begin(), recent_competence.end(), 0.0f) / recent_competence.size();
        float variance = 0.0f;
        for (float comp : recent_competence) {
            variance += (comp - mean) * (comp - mean);
        }
        variance /= recent_competence.size();
        
        // Low variance indicates stagnation, which increases exploration motivation
        learning_stagnation = 1.0f - std::clamp(variance * 10.0f, 0.0f, 1.0f);
    }
    
    float exploration = (uncertainty * 0.4f) + 
                       (novelty_potential * 0.3f) + 
                       (learning_stagnation * 0.3f);
    
    return std::clamp(exploration, 0.0f, 1.0f);
}

float IntrinsicMotivationSystem::calculateMetaLearningMotivation() {
    if (!brain_) return 0.0f;
    
    // Calculate motivation for learning how to learn better
    // Based on learning efficiency trends and adaptation capability
    
    float learning_efficiency = 0.0f;
    if (motivation_history_.size() >= 3) {
        // Calculate learning rate improvement over time
        float recent_competence = motivation_history_.back().competence;
        float past_competence = motivation_history_[motivation_history_.size() - 3].competence;
        learning_efficiency = std::max(0.0f, recent_competence - past_competence);
    }
    
    // Meta-learning motivation is higher when learning efficiency is low
    float meta_motivation = 1.0f - learning_efficiency;
    
    return std::clamp(meta_motivation, 0.0f, 1.0f);
}

float IntrinsicMotivationSystem::generateIntrinsicReward(const MotivationComponents& motivation_components) {
    // Generate reward signal based on motivation components
    float reward = motivation_components.composite;
    
    // Add exploration bonus if exploration motivation is high
    if (motivation_components.exploration > 0.7f) {
        reward += config_.exploration_bonus;
    }
    
    // Scale reward to appropriate range
    return std::clamp(reward, 0.0f, 1.0f);
}

float IntrinsicMotivationSystem::detectSurprise(const std::vector<float>& current_state) {
    if (current_state.empty() || state_history_.empty()) {
        return 0.0f;
    }
    
    // Calculate surprise as deviation from expected state
    // Use recent state history to predict current state
    
    std::vector<float> predicted_state(current_state.size(), 0.0f);
    
    // Simple prediction: weighted average of recent states
    float total_weight = 0.0f;
    std::size_t history_window = std::min(static_cast<std::size_t>(5), state_history_.size());
    
    for (std::size_t i = state_history_.size() - history_window; i < state_history_.size(); ++i) {
        float weight = static_cast<float>(i + 1); // More recent states have higher weight
        total_weight += weight;
        
        for (std::size_t j = 0; j < std::min(predicted_state.size(), state_history_[i].size()); ++j) {
            predicted_state[j] += state_history_[i][j] * weight;
        }
    }
    
    // Normalize prediction
    if (total_weight > 0.0f) {
        for (float& pred : predicted_state) {
            pred /= total_weight;
        }
    }
    
    // Calculate surprise as prediction error
    float surprise = 0.0f;
    for (std::size_t i = 0; i < std::min(current_state.size(), predicted_state.size()); ++i) {
        float error = std::abs(current_state[i] - predicted_state[i]);
        surprise += error;
    }
    
    if (!current_state.empty()) {
        surprise /= current_state.size();
    }
    
    return std::clamp(surprise, 0.0f, 1.0f);
}

void IntrinsicMotivationSystem::updatePredictionTracker(float prediction, float actual_outcome) {
    std::lock_guard<std::mutex> lock(motivation_mutex_);
    
    // Add new prediction and outcome
    prediction_tracker_.predictions.push_back(prediction);
    prediction_tracker_.actual_outcomes.push_back(actual_outcome);
    
    // Calculate error
    float error = std::abs(prediction - actual_outcome);
    prediction_tracker_.errors.push_back(error);
    
    // Maintain window size
    if (prediction_tracker_.predictions.size() > config_.prediction_window) {
        prediction_tracker_.predictions.erase(prediction_tracker_.predictions.begin());
        prediction_tracker_.actual_outcomes.erase(prediction_tracker_.actual_outcomes.begin());
        prediction_tracker_.errors.erase(prediction_tracker_.errors.begin());
    }
    
    // Update statistics
    if (!prediction_tracker_.errors.empty()) {
        prediction_tracker_.average_error = std::accumulate(
            prediction_tracker_.errors.begin(), 
            prediction_tracker_.errors.end(), 0.0f) / prediction_tracker_.errors.size();
        
        // Calculate variance
        float variance = 0.0f;
        for (float err : prediction_tracker_.errors) {
            float diff = err - prediction_tracker_.average_error;
            variance += diff * diff;
        }
        prediction_tracker_.error_variance = variance / prediction_tracker_.errors.size();
    }
}

float IntrinsicMotivationSystem::updateNoveltyDetector(const std::vector<float>& experience) {
    if (experience.empty()) return 0.0f;
    
    std::lock_guard<std::mutex> lock(motivation_mutex_);
    
    float novelty_score = 0.0f;
    
    if (novelty_detector_.experience_memory.empty()) {
        // First experience is always novel
        novelty_score = 1.0f;
    } else {
        // Calculate similarity to past experiences
        float max_similarity = 0.0f;
        
        for (const auto& past_experience : novelty_detector_.experience_memory) {
            if (past_experience.size() != experience.size()) continue;
            
            // Calculate cosine similarity
            float dot_product = 0.0f;
            float norm_past = 0.0f;
            float norm_current = 0.0f;
            
            for (std::size_t i = 0; i < experience.size(); ++i) {
                dot_product += past_experience[i] * experience[i];
                norm_past += past_experience[i] * past_experience[i];
                norm_current += experience[i] * experience[i];
            }
            
            if (norm_past > 0.0f && norm_current > 0.0f) {
                float similarity = dot_product / (std::sqrt(norm_past) * std::sqrt(norm_current));
                max_similarity = std::max(max_similarity, similarity);
            }
        }
        
        // Novelty is inverse of similarity
        novelty_score = 1.0f - max_similarity;
    }
    
    // Add to memory if sufficiently novel
    if (novelty_score > novelty_detector_.novelty_threshold) {
        novelty_detector_.experience_memory.push_back(experience);
        
        // Maintain memory capacity
        if (novelty_detector_.experience_memory.size() > novelty_detector_.memory_capacity) {
            novelty_detector_.experience_memory.erase(novelty_detector_.experience_memory.begin());
        }
    }
    
    // Update novelty scores
    novelty_detector_.novelty_scores.push_back(novelty_score);
    if (novelty_detector_.novelty_scores.size() > 20) { // Keep last 20 scores
        novelty_detector_.novelty_scores.erase(novelty_detector_.novelty_scores.begin());
    }
    
    return novelty_score;
}

std::vector<IntrinsicMotivationSystem::MotivationComponents> 
IntrinsicMotivationSystem::getMotivationHistory(std::size_t count) const {
    std::lock_guard<std::mutex> lock(motivation_mutex_);
    
    std::vector<MotivationComponents> history;
    std::size_t start_index = motivation_history_.size() > count ? 
        motivation_history_.size() - count : 0;
    
    for (std::size_t i = start_index; i < motivation_history_.size(); ++i) {
        history.push_back(motivation_history_[i]);
    }
    
    return history;
}

float IntrinsicMotivationSystem::calculateActivationVariance() const {
    if (!brain_) return 0.0f;
    
    std::vector<float> activations;
    const auto& regions = brain_->getRegionsMap();
    
    for (const auto& region_pair : regions) {
        if (region_pair.second) {
            activations.push_back(region_pair.second->getGlobalActivation());
        }
    }
    
    if (activations.empty()) return 0.0f;
    
    float mean = std::accumulate(activations.begin(), activations.end(), 0.0f) / activations.size();
    float variance = 0.0f;
    
    for (float activation : activations) {
        float diff = activation - mean;
        variance += diff * diff;
    }
    
    variance /= activations.size();
    return std::sqrt(variance); // Return standard deviation
}

float IntrinsicMotivationSystem::calculateLearningRateDynamics() const {
    if (!brain_) return 0.0f;
    
    auto* learning_system = brain_->getLearningSystem();
    if (!learning_system) return 0.0f;
    
    float current_lr = learning_system->getConfig().global_learning_rate;
    
    // Calculate dynamics based on learning rate changes
    // For now, use current learning rate as proxy for dynamics
    return std::clamp(current_lr, 0.0f, 1.0f);
}

float IntrinsicMotivationSystem::calculateSubstrateComplexity() const {
    if (!brain_) return 0.0f;
    
    // Calculate complexity based on:
    // 1. Number of active regions
    // 2. Connectivity patterns
    // 3. Activation diversity
    
    const auto& regions = brain_->getRegionsMap();
    float active_regions = 0.0f;
    float total_activation = 0.0f;
    
    for (const auto& region_pair : regions) {
            if (region_pair.second) {
                float activation = region_pair.second->getGlobalActivation();
                if (activation > 0.1f) { // Consider region active if activation > threshold
                    active_regions += 1.0f;
                }
                total_activation += activation;
            }
        }
    
    float region_count = static_cast<float>(regions.size());
    if (region_count == 0.0f) return 0.0f;
    
    float complexity = (active_regions / region_count) * std::min(1.0f, total_activation / region_count);
    return std::clamp(complexity, 0.0f, 1.0f);
}

void IntrinsicMotivationSystem::updateMotivationHistory(const MotivationComponents& components) {
    motivation_history_.push_back(components);
    
    // Maintain history size (keep last 100 entries)
    if (motivation_history_.size() > 100) {
        motivation_history_.erase(motivation_history_.begin());
    }
}

void IntrinsicMotivationSystem::applyMotivationDecay(float delta_time) {
    float decay_factor = std::pow(config_.motivation_decay_rate, delta_time);
    
    current_motivation_.uncertainty *= decay_factor;
    current_motivation_.prediction_error *= decay_factor;
    current_motivation_.novelty *= decay_factor;
    current_motivation_.curiosity *= decay_factor;
    current_motivation_.competence *= decay_factor;
    current_motivation_.exploration *= decay_factor;
    current_motivation_.meta_learning *= decay_factor;
}

void IntrinsicMotivationSystem::normalizeMotivationComponents(MotivationComponents& components) {
    // Ensure all components are in [0, 1] range
    components.uncertainty = std::clamp(components.uncertainty, 0.0f, 1.0f);
    components.prediction_error = std::clamp(components.prediction_error, 0.0f, 1.0f);
    components.novelty = std::clamp(components.novelty, 0.0f, 1.0f);
    components.curiosity = std::clamp(components.curiosity, 0.0f, 1.0f);
    components.competence = std::clamp(components.competence, 0.0f, 1.0f);
    components.exploration = std::clamp(components.exploration, 0.0f, 1.0f);
    components.meta_learning = std::clamp(components.meta_learning, 0.0f, 1.0f);
}

float IntrinsicMotivationSystem::calculateCompositeMotivation(const MotivationComponents& components) {
    // Weighted combination of motivation components
    float composite = (components.uncertainty * config_.uncertainty_weight) +
                     (components.prediction_error * config_.prediction_error_weight) +
                     (components.novelty * config_.novelty_weight) +
                     (components.curiosity * config_.curiosity_weight) +
                     (components.competence * 0.1f) +  // Fixed small weight
                     (components.exploration * 0.1f) +  // Fixed small weight
                     (components.meta_learning * 0.05f); // Fixed small weight
    
    return std::clamp(composite, 0.0f, 1.0f);
}

} // namespace Core
} // namespace NeuroForge