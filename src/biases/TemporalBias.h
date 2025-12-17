#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <cstdint>
#include <functional>

namespace NeuroForge {
namespace Biases {

/**
 * @brief TemporalBias implements temporal processing mechanisms including
 * rhythm detection, temporal pattern recognition, and circadian alignment.
 * 
 * This bias system models temporal cortex functionality and circadian rhythms,
 * providing the neural substrate with sophisticated time-based processing
 * capabilities essential for biological neural networks.
 */
class TemporalBias {
public:
    struct Config {
        // Rhythm detection parameters
        int rhythm_detector_count = 64;           // Number of rhythm detectors
        float min_rhythm_frequency = 0.1f;        // Minimum detectable frequency (Hz)
        float max_rhythm_frequency = 100.0f;      // Maximum detectable frequency (Hz)
        float rhythm_detection_threshold = 0.7f;  // Rhythm detection threshold
        float rhythm_adaptation_rate = 0.05f;     // Rhythm adaptation learning rate
        
        // Temporal pattern parameters
        int max_pattern_length = 32;              // Maximum temporal pattern length
        int pattern_memory_capacity = 256;        // Pattern memory capacity
        float pattern_similarity_threshold = 0.8f; // Pattern matching threshold
        float temporal_decay_rate = 0.95f;        // Temporal memory decay rate
        
        // Circadian rhythm parameters
        float circadian_period = 24.0f * 3600.0f; // Circadian period (seconds)
        float circadian_amplitude = 1.0f;         // Circadian oscillation amplitude
        float circadian_phase_shift = 0.0f;       // Phase shift (radians)
        float light_sensitivity = 0.3f;           // Light entrainment sensitivity
        
        // Temporal integration parameters
        float temporal_integration_window = 1000.0f; // Integration window (ms)
        float temporal_resolution = 10.0f;        // Temporal resolution (ms)
        int max_temporal_contexts = 16;           // Maximum temporal contexts
        
        // Learning parameters
        float temporal_learning_rate = 0.1f;      // Temporal learning rate
        float pattern_reinforcement_strength = 0.2f; // Pattern reinforcement
        bool enable_predictive_coding = true;     // Enable temporal prediction
        bool enable_sequence_learning = true;     // Enable sequence learning
    };
    
    struct TemporalEvent {
        float timestamp;                          // Event timestamp (ms)
        float intensity;                          // Event intensity
        std::uint32_t event_type;                // Event type identifier
        std::vector<float> features;             // Event feature vector
        
        TemporalEvent() : timestamp(0), intensity(0), event_type(0) {}
        TemporalEvent(float ts, float intens, std::uint32_t type, const std::vector<float>& feat)
            : timestamp(ts), intensity(intens), event_type(type), features(feat) {}
    };
    
    struct RhythmDetector {
        float frequency;                          // Target frequency (Hz)
        float phase;                             // Current phase (radians)
        float amplitude;                         // Current amplitude
        float confidence;                        // Detection confidence
        std::vector<float> history;             // Recent activation history
        bool is_active;                         // Detector active state
        
        RhythmDetector() : frequency(0), phase(0), amplitude(0), confidence(0), is_active(false) {}
    };
    
    struct TemporalPattern {
        std::vector<TemporalEvent> sequence;     // Event sequence
        float pattern_strength;                  // Pattern strength
        float last_activation;                   // Last activation time
        int occurrence_count;                    // Pattern occurrence count
        std::vector<float> prediction_weights;   // Predictive weights
        
        TemporalPattern() : pattern_strength(0), last_activation(0), occurrence_count(0) {}
    };
    
    struct CircadianState {
        float current_phase;                     // Current circadian phase
        float amplitude;                         // Current amplitude
        float light_level;                       // Current light level
        float temperature_factor;                // Temperature influence
        float melatonin_level;                   // Simulated melatonin level
        float cortisol_level;                    // Simulated cortisol level
        
        CircadianState() : current_phase(0), amplitude(1), light_level(0.5f), 
                          temperature_factor(1), melatonin_level(0), cortisol_level(0) {}
    };
    
    struct TemporalContext {
        std::vector<TemporalEvent> recent_events; // Recent event buffer
        std::vector<float> temporal_features;     // Extracted temporal features
        float context_coherence;                  // Context coherence measure
        float prediction_accuracy;                // Prediction accuracy
        
        TemporalContext() : context_coherence(0), prediction_accuracy(0) {}
    };

public:
    explicit TemporalBias(const Config& config);
    ~TemporalBias() = default;
    
    // Core temporal processing
    void processTemporalEvent(const TemporalEvent& event);
    void updateRhythmDetectors(float current_time);
    void updateCircadianRhythm(float current_time, float light_level = 0.5f);
    
    // Pattern recognition and learning
    void learnTemporalPattern(const std::vector<TemporalEvent>& sequence);
    std::vector<int> recognizePatterns(const std::vector<TemporalEvent>& sequence);
    std::vector<TemporalEvent> predictNextEvents(float prediction_horizon = 1000.0f);
    
    // Rhythm analysis
    std::vector<float> detectRhythms(const std::vector<float>& signal, float sampling_rate);
    float getRhythmStrength(float frequency) const;
    std::vector<RhythmDetector> getActiveRhythms() const;
    
    // Circadian functions
    float getCircadianPhase() const;
    float getCircadianAmplitude() const;
    void entrainToLight(float light_intensity, float duration);
    void simulateJetLag(float time_shift_hours);
    
    // Temporal context management
    void updateTemporalContext(const TemporalEvent& event);
    TemporalContext getCurrentContext() const;
    float getTemporalCoherence() const;
    
    // Configuration and state
    void updateConfig(const Config& new_config);
    Config getConfig() const;
    void reset();
    
    // Analysis and metrics
    float getTemporalComplexity() const;
    float getPredictionAccuracy() const;
    std::vector<float> getTemporalFeatures() const;

private:
    Config config_;
    mutable std::mutex bias_mutex_;
    
    // Rhythm detection state
    std::vector<RhythmDetector> rhythm_detectors_;
    std::vector<float> signal_buffer_;
    float last_update_time_;
    
    // Pattern recognition state
    std::vector<TemporalPattern> learned_patterns_;
    std::vector<TemporalEvent> event_history_;
    
    // Circadian state
    CircadianState circadian_state_;
    float circadian_start_time_;
    
    // Temporal context
    std::vector<TemporalContext> temporal_contexts_;
    int current_context_index_;
    
    // Internal processing methods
    void initializeRhythmDetectors();
    void updateRhythmDetector(RhythmDetector& detector, float signal_value, float current_time);
    float calculateRhythmActivation(const RhythmDetector& detector, float frequency) const;
    
    void processPatternLearning(const std::vector<TemporalEvent>& sequence);
    float calculatePatternSimilarity(const TemporalPattern& pattern, 
                                   const std::vector<TemporalEvent>& sequence) const;
    
    void updateCircadianOscillator(float current_time, float light_input);
    float calculateCircadianOutput(float phase) const;
    
    void extractTemporalFeatures(const std::vector<TemporalEvent>& events, 
                               std::vector<float>& features) const;
    float calculateTemporalCoherence(const std::vector<TemporalEvent>& events) const;
};

} // namespace Biases
} // namespace NeuroForge