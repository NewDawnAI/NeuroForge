#include "TemporalBias.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NeuroForge {
namespace Biases {

TemporalBias::TemporalBias(const Config& config)
    : config_(config)
    , last_update_time_(0.0f)
    , circadian_start_time_(0.0f)
    , current_context_index_(0) {
    
    initializeRhythmDetectors();
    
    // Initialize temporal contexts
    temporal_contexts_.resize(config_.max_temporal_contexts);
    
    // Initialize circadian state
    circadian_state_ = CircadianState{};
    
    // Initialize signal buffer
    signal_buffer_.reserve(1000);
}

void TemporalBias::processTemporalEvent(const TemporalEvent& event) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    // Add to event history
    event_history_.push_back(event);
    
    // Maintain history size
    if (event_history_.size() > static_cast<size_t>(config_.pattern_memory_capacity)) {
        event_history_.erase(event_history_.begin());
    }
    
    // Update temporal context
    updateTemporalContext(event);
    
    // Update rhythm detectors with event intensity
    updateRhythmDetectors(event.timestamp);
    signal_buffer_.push_back(event.intensity);
    
    // Maintain signal buffer size
    if (signal_buffer_.size() > 1000) {
        signal_buffer_.erase(signal_buffer_.begin());
    }
    
    // Check for pattern recognition
    if (event_history_.size() >= 3) {
        std::vector<TemporalEvent> recent_sequence(
            event_history_.end() - std::min(static_cast<int>(event_history_.size()), 
                                          config_.max_pattern_length),
            event_history_.end()
        );
        recognizePatterns(recent_sequence);
    }
}

void TemporalBias::updateRhythmDetectors(float current_time) {
    float dt = current_time - last_update_time_;
    if (dt <= 0) return;
    
    last_update_time_ = current_time;
    
    // Get current signal value
    float signal_value = signal_buffer_.empty() ? 0.0f : signal_buffer_.back();
    
    // Update each rhythm detector
    for (auto& detector : rhythm_detectors_) {
        updateRhythmDetector(detector, signal_value, current_time);
    }
}

void TemporalBias::updateCircadianRhythm(float current_time, float light_level) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    if (circadian_start_time_ == 0.0f) {
        circadian_start_time_ = current_time;
    }
    
    circadian_state_.light_level = light_level;
    updateCircadianOscillator(current_time, light_level);
}

void TemporalBias::learnTemporalPattern(const std::vector<TemporalEvent>& sequence) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    if (sequence.empty() || sequence.size() > static_cast<size_t>(config_.max_pattern_length)) {
        return;
    }
    
    // Check if pattern already exists
    for (auto& pattern : learned_patterns_) {
        float similarity = calculatePatternSimilarity(pattern, sequence);
        if (similarity > config_.pattern_similarity_threshold) {
            // Reinforce existing pattern
            pattern.pattern_strength += config_.pattern_reinforcement_strength;
            pattern.occurrence_count++;
            pattern.last_activation = sequence.back().timestamp;
            return;
        }
    }
    
    // Create new pattern
    if (learned_patterns_.size() < static_cast<size_t>(config_.pattern_memory_capacity)) {
        TemporalPattern new_pattern;
        new_pattern.sequence = sequence;
        new_pattern.pattern_strength = 1.0f;
        new_pattern.occurrence_count = 1;
        new_pattern.last_activation = sequence.back().timestamp;
        new_pattern.prediction_weights.resize(sequence.size(), 1.0f);
        
        learned_patterns_.push_back(new_pattern);
    }
}

std::vector<int> TemporalBias::recognizePatterns(const std::vector<TemporalEvent>& sequence) {
    std::vector<int> recognized_patterns;
    
    for (size_t i = 0; i < learned_patterns_.size(); ++i) {
        float similarity = calculatePatternSimilarity(learned_patterns_[i], sequence);
        if (similarity > config_.pattern_similarity_threshold) {
            recognized_patterns.push_back(static_cast<int>(i));
            
            // Update pattern activation
            learned_patterns_[i].last_activation = sequence.back().timestamp;
            learned_patterns_[i].pattern_strength *= (1.0f + config_.temporal_learning_rate);
        }
    }
    
    return recognized_patterns;
}

std::vector<TemporalBias::TemporalEvent> TemporalBias::predictNextEvents(float prediction_horizon) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    std::vector<TemporalEvent> predictions;
    
    if (!config_.enable_predictive_coding || learned_patterns_.empty()) {
        return predictions;
    }
    
    // Find most relevant patterns based on recent history
    std::vector<std::pair<float, int>> pattern_relevance;
    
    for (size_t i = 0; i < learned_patterns_.size(); ++i) {
        const auto& pattern = learned_patterns_[i];
        
        // Calculate relevance based on pattern strength and recency
        float time_factor = std::exp(-(last_update_time_ - pattern.last_activation) / 1000.0f);
        float relevance = pattern.pattern_strength * time_factor;
        
        pattern_relevance.emplace_back(relevance, static_cast<int>(i));
    }
    
    // Sort by relevance
    std::sort(pattern_relevance.rbegin(), pattern_relevance.rend());
    
    // Generate predictions from top patterns
    int max_predictions = std::min(3, static_cast<int>(pattern_relevance.size()));
    for (int i = 0; i < max_predictions; ++i) {
        int pattern_idx = pattern_relevance[i].second;
        const auto& pattern = learned_patterns_[pattern_idx];
        
        if (!pattern.sequence.empty()) {
            // Predict next event based on pattern
            TemporalEvent predicted_event = pattern.sequence.back();
            predicted_event.timestamp = last_update_time_ + prediction_horizon * (i + 1) / max_predictions;
            
            // Ensure intensity stays within valid range [0, 1]
            float weighted_intensity = predicted_event.intensity * pattern_relevance[i].first;
            predicted_event.intensity = std::max(0.0f, std::min(1.0f, weighted_intensity));
            
            predictions.push_back(predicted_event);
        }
    }
    
    return predictions;
}

std::vector<float> TemporalBias::detectRhythms(const std::vector<float>& signal, float sampling_rate) {
    std::vector<float> detected_frequencies;
    
    if (signal.size() < 10) return detected_frequencies;
    
    // Simple frequency analysis using autocorrelation
    int max_lag = std::min(static_cast<int>(signal.size() / 2), 100);
    std::vector<float> autocorr(max_lag);
    
    for (int lag = 1; lag < max_lag; ++lag) {
        float sum = 0.0f;
        int count = 0;
        
        for (size_t i = lag; i < signal.size(); ++i) {
            sum += signal[i] * signal[i - lag];
            count++;
        }
        
        autocorr[lag] = count > 0 ? sum / count : 0.0f;
    }
    
    // Find peaks in autocorrelation
    for (int i = 2; i < max_lag - 2; ++i) {
        if (autocorr[i] > autocorr[i-1] && autocorr[i] > autocorr[i+1] &&
            autocorr[i] > config_.rhythm_detection_threshold) {
            
            float frequency = sampling_rate / i;
            if (frequency >= config_.min_rhythm_frequency && 
                frequency <= config_.max_rhythm_frequency) {
                detected_frequencies.push_back(frequency);
            }
        }
    }
    
    return detected_frequencies;
}

float TemporalBias::getRhythmStrength(float frequency) const {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    float max_strength = 0.0f;
    for (const auto& detector : rhythm_detectors_) {
        if (std::abs(detector.frequency - frequency) < 0.1f) {
            max_strength = std::max(max_strength, detector.confidence);
        }
    }
    
    return max_strength;
}

std::vector<TemporalBias::RhythmDetector> TemporalBias::getActiveRhythms() const {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    std::vector<RhythmDetector> active_rhythms;
    for (const auto& detector : rhythm_detectors_) {
        if (detector.is_active && detector.confidence > config_.rhythm_detection_threshold) {
            active_rhythms.push_back(detector);
        }
    }
    
    return active_rhythms;
}

float TemporalBias::getCircadianPhase() const {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    return circadian_state_.current_phase;
}

float TemporalBias::getCircadianAmplitude() const {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    return circadian_state_.amplitude;
}

void TemporalBias::entrainToLight(float light_intensity, float duration) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    // Simulate light entrainment effect
    float phase_shift = light_intensity * config_.light_sensitivity * duration / 3600.0f;
    circadian_state_.current_phase += phase_shift;
    
    // Normalize phase
    while (circadian_state_.current_phase > 2.0f * M_PI) {
        circadian_state_.current_phase -= 2.0f * M_PI;
    }
    while (circadian_state_.current_phase < 0) {
        circadian_state_.current_phase += 2.0f * M_PI;
    }
}

void TemporalBias::simulateJetLag(float time_shift_hours) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    // Shift circadian phase
    float phase_shift = (time_shift_hours / 24.0f) * 2.0f * M_PI;
    circadian_state_.current_phase += phase_shift;
    
    // Normalize phase
    while (circadian_state_.current_phase > 2.0f * M_PI) {
        circadian_state_.current_phase -= 2.0f * M_PI;
    }
    while (circadian_state_.current_phase < 0) {
        circadian_state_.current_phase += 2.0f * M_PI;
    }
    
    // Reduce amplitude temporarily (jet lag effect)
    circadian_state_.amplitude *= 0.7f;
}

void TemporalBias::updateTemporalContext(const TemporalEvent& event) {
    if (temporal_contexts_.empty()) return;
    
    TemporalContext& context = temporal_contexts_[current_context_index_];
    
    // Add event to recent events
    context.recent_events.push_back(event);
    
    // Maintain context window size
    float window_start = event.timestamp - config_.temporal_integration_window;
    context.recent_events.erase(
        std::remove_if(context.recent_events.begin(), context.recent_events.end(),
                      [window_start](const TemporalEvent& e) { return e.timestamp < window_start; }),
        context.recent_events.end()
    );
    
    // Extract temporal features
    extractTemporalFeatures(context.recent_events, context.temporal_features);
    
    // Calculate context coherence
    context.context_coherence = calculateTemporalCoherence(context.recent_events);
}

TemporalBias::TemporalContext TemporalBias::getCurrentContext() const {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    if (temporal_contexts_.empty()) return TemporalContext{};
    return temporal_contexts_[current_context_index_];
}

float TemporalBias::getTemporalCoherence() const {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    if (temporal_contexts_.empty()) return 0.0f;
    return temporal_contexts_[current_context_index_].context_coherence;
}

void TemporalBias::updateConfig(const Config& new_config) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    config_ = new_config;
    
    // Reinitialize rhythm detectors if needed
    if (rhythm_detectors_.size() != static_cast<size_t>(config_.rhythm_detector_count)) {
        initializeRhythmDetectors();
    }
    
    // Resize temporal contexts if needed
    if (temporal_contexts_.size() != static_cast<size_t>(config_.max_temporal_contexts)) {
        temporal_contexts_.resize(config_.max_temporal_contexts);
        current_context_index_ = 0;
    }
}

TemporalBias::Config TemporalBias::getConfig() const {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    return config_;
}

void TemporalBias::reset() {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    // Reset rhythm detectors
    initializeRhythmDetectors();
    
    // Clear patterns and history
    learned_patterns_.clear();
    event_history_.clear();
    signal_buffer_.clear();
    
    // Reset circadian state
    circadian_state_ = CircadianState{};
    circadian_start_time_ = 0.0f;
    
    // Reset temporal contexts
    temporal_contexts_.clear();
    temporal_contexts_.resize(config_.max_temporal_contexts);
    current_context_index_ = 0;
    
    last_update_time_ = 0.0f;
}

float TemporalBias::getTemporalComplexity() const {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    // Calculate complexity based on active patterns and rhythms
    float pattern_complexity = static_cast<float>(learned_patterns_.size()) / config_.pattern_memory_capacity;
    
    int active_rhythms = 0;
    for (const auto& detector : rhythm_detectors_) {
        if (detector.is_active) active_rhythms++;
    }
    float rhythm_complexity = static_cast<float>(active_rhythms) / rhythm_detectors_.size();
    
    return (pattern_complexity + rhythm_complexity) / 2.0f;
}

float TemporalBias::getPredictionAccuracy() const {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    if (temporal_contexts_.empty()) return 0.0f;
    return temporal_contexts_[current_context_index_].prediction_accuracy;
}

std::vector<float> TemporalBias::getTemporalFeatures() const {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    if (temporal_contexts_.empty()) return std::vector<float>();
    return temporal_contexts_[current_context_index_].temporal_features;
}

// Private methods implementation

void TemporalBias::initializeRhythmDetectors() {
    rhythm_detectors_.clear();
    rhythm_detectors_.resize(config_.rhythm_detector_count);
    
    // Initialize detectors with logarithmically spaced frequencies
    float log_min = std::log(config_.min_rhythm_frequency);
    float log_max = std::log(config_.max_rhythm_frequency);
    float log_step = (log_max - log_min) / config_.rhythm_detector_count;
    
    for (int i = 0; i < config_.rhythm_detector_count; ++i) {
        auto& detector = rhythm_detectors_[i];
        detector.frequency = std::exp(log_min + i * log_step);
        detector.phase = 0.0f;
        detector.amplitude = 0.0f;
        detector.confidence = 0.0f;
        detector.is_active = false;
        detector.history.reserve(100);
    }
}

void TemporalBias::updateRhythmDetector(RhythmDetector& detector, float signal_value, float current_time) {
    // Update phase based on frequency
    float dt = current_time - last_update_time_;
    detector.phase += 2.0f * M_PI * detector.frequency * dt / 1000.0f; // dt in ms
    
    // Normalize phase
    while (detector.phase > 2.0f * M_PI) {
        detector.phase -= 2.0f * M_PI;
    }
    
    // Calculate expected signal based on current phase
    float expected_signal = detector.amplitude * std::sin(detector.phase);
    
    // Update amplitude and confidence based on signal match
    float error = std::abs(signal_value - expected_signal);
    float match_strength = std::exp(-error);
    
    // Adaptive learning
    detector.amplitude += config_.rhythm_adaptation_rate * (signal_value - detector.amplitude);
    detector.confidence += config_.rhythm_adaptation_rate * (match_strength - detector.confidence);
    
    // Update history
    detector.history.push_back(match_strength);
    if (detector.history.size() > 100) {
        detector.history.erase(detector.history.begin());
    }
    
    // Determine if detector is active
    detector.is_active = detector.confidence > config_.rhythm_detection_threshold;
}

float TemporalBias::calculateRhythmActivation(const RhythmDetector& detector, float frequency) const {
    float freq_diff = std::abs(detector.frequency - frequency);
    float freq_similarity = std::exp(-freq_diff / (detector.frequency * 0.1f));
    return detector.confidence * freq_similarity;
}

void TemporalBias::processPatternLearning(const std::vector<TemporalEvent>& sequence) {
    if (!config_.enable_sequence_learning) return;
    
    // Extract subsequences for learning
    for (size_t start = 0; start < sequence.size(); ++start) {
        for (size_t length = 2; length <= std::min(sequence.size() - start, 
                                                  static_cast<size_t>(config_.max_pattern_length)); ++length) {
            std::vector<TemporalEvent> subsequence(sequence.begin() + start, 
                                                  sequence.begin() + start + length);
            learnTemporalPattern(subsequence);
        }
    }
}

float TemporalBias::calculatePatternSimilarity(const TemporalPattern& pattern, 
                                             const std::vector<TemporalEvent>& sequence) const {
    if (pattern.sequence.size() != sequence.size()) {
        return 0.0f;
    }
    
    float similarity = 0.0f;
    float total_weight = 0.0f;
    
    for (size_t i = 0; i < pattern.sequence.size(); ++i) {
        const auto& p_event = pattern.sequence[i];
        const auto& s_event = sequence[i];
        
        // Compare event types
        float type_similarity = (p_event.event_type == s_event.event_type) ? 1.0f : 0.0f;
        
        // Compare intensities
        float intensity_diff = std::abs(p_event.intensity - s_event.intensity);
        float intensity_similarity = std::exp(-intensity_diff);
        
        // Compare features if available
        float feature_similarity = 1.0f;
        if (!p_event.features.empty() && !s_event.features.empty() && 
            p_event.features.size() == s_event.features.size()) {
            
            float feature_sum = 0.0f;
            for (size_t j = 0; j < p_event.features.size(); ++j) {
                float diff = std::abs(p_event.features[j] - s_event.features[j]);
                feature_sum += std::exp(-diff);
            }
            feature_similarity = feature_sum / p_event.features.size();
        }
        
        float event_similarity = (type_similarity + intensity_similarity + feature_similarity) / 3.0f;
        float weight = 1.0f; // Could be based on temporal position or importance
        
        similarity += event_similarity * weight;
        total_weight += weight;
    }
    
    return total_weight > 0 ? similarity / total_weight : 0.0f;
}

void TemporalBias::updateCircadianOscillator(float current_time, float light_input) {
    float elapsed_time = current_time - circadian_start_time_;
    
    // Basic circadian oscillator with light entrainment
    float natural_phase = (elapsed_time / config_.circadian_period) * 2.0f * M_PI;
    float light_effect = light_input * config_.light_sensitivity;
    
    circadian_state_.current_phase = natural_phase + config_.circadian_phase_shift + light_effect;
    
    // Normalize phase
    while (circadian_state_.current_phase > 2.0f * M_PI) {
        circadian_state_.current_phase -= 2.0f * M_PI;
    }
    
    // Calculate hormone levels based on circadian phase
    circadian_state_.melatonin_level = std::max(0.0f, static_cast<float>(-std::cos(circadian_state_.current_phase)));
    circadian_state_.cortisol_level = std::max(0.0f, static_cast<float>(std::cos(circadian_state_.current_phase + M_PI / 2)));
    
    // Update amplitude (can be affected by jet lag, etc.)
    circadian_state_.amplitude = std::min(config_.circadian_amplitude, 
                                        circadian_state_.amplitude * 1.01f); // Gradual recovery
}

float TemporalBias::calculateCircadianOutput(float phase) const {
    return circadian_state_.amplitude * std::sin(phase);
}

void TemporalBias::extractTemporalFeatures(const std::vector<TemporalEvent>& events, 
                                         std::vector<float>& features) const {
    features.clear();
    
    if (events.empty()) return;
    
    // Basic temporal features
    features.push_back(static_cast<float>(events.size())); // Event count
    
    // Temporal statistics
    if (events.size() > 1) {
        std::vector<float> intervals;
        for (size_t i = 1; i < events.size(); ++i) {
            intervals.push_back(events[i].timestamp - events[i-1].timestamp);
        }
        
        // Mean interval
        float mean_interval = std::accumulate(intervals.begin(), intervals.end(), 0.0f) / intervals.size();
        features.push_back(mean_interval);
        
        // Interval variance
        float variance = 0.0f;
        for (float interval : intervals) {
            variance += (interval - mean_interval) * (interval - mean_interval);
        }
        variance /= intervals.size();
        features.push_back(variance);
    } else {
        features.push_back(0.0f); // Mean interval
        features.push_back(0.0f); // Variance
    }
    
    // Intensity statistics
    std::vector<float> intensities;
    for (const auto& event : events) {
        intensities.push_back(event.intensity);
    }
    
    float mean_intensity = std::accumulate(intensities.begin(), intensities.end(), 0.0f) / intensities.size();
    features.push_back(mean_intensity);
    
    auto minmax = std::minmax_element(intensities.begin(), intensities.end());
    features.push_back(*minmax.second - *minmax.first); // Intensity range
}

float TemporalBias::calculateTemporalCoherence(const std::vector<TemporalEvent>& events) const {
    if (events.size() < 2) return 0.0f;
    
    // Calculate coherence based on temporal regularity and intensity patterns
    std::vector<float> intervals;
    for (size_t i = 1; i < events.size(); ++i) {
        intervals.push_back(events[i].timestamp - events[i-1].timestamp);
    }
    
    // Regularity measure (inverse of coefficient of variation)
    float mean_interval = std::accumulate(intervals.begin(), intervals.end(), 0.0f) / intervals.size();
    float variance = 0.0f;
    for (float interval : intervals) {
        variance += (interval - mean_interval) * (interval - mean_interval);
    }
    variance /= intervals.size();
    
    float cv = (mean_interval > 0) ? std::sqrt(variance) / mean_interval : 1.0f;
    float regularity = 1.0f / (1.0f + cv);
    
    return regularity;
}

} // namespace Biases
} // namespace NeuroForge