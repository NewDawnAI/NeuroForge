#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <numeric>

#ifndef NF_HAVE_OPENCV
// Forward declarations for OpenCV types when not available
namespace cv {
    struct Point2f;
    class Mat;
}
#endif

namespace NeuroForge {
namespace Biases {

/**
 * @brief Enhanced Novelty Detection System
 * 
 * Implements structured curiosity beyond current threshold system with
 * prediction-error driven exploration and information-seeking behaviors.
 * Based on biological curiosity mechanisms and intrinsic motivation.
 */
class NoveltyBias {
public:
    /**
     * @brief Novelty detection metrics
     */
    struct NoveltyMetrics {
        float prediction_error{0.0f};      // Prediction vs reality mismatch
        float information_gain{0.0f};      // Expected information from exploration
        float surprise_level{0.0f};        // Unexpected event magnitude
        float exploration_bonus{0.0f};     // Intrinsic motivation reward
        float familiarity_score{0.0f};     // How familiar this input is
        float complexity_score{0.0f};      // Input complexity assessment
    };
    
    /**
     * @brief Configuration parameters for novelty detection
     */
    struct Config {
        size_t experience_buffer_size{1000};       // Maximum experiences to remember
        float novelty_threshold{0.3f};             // Threshold for novelty detection
        float prediction_learning_rate{0.1f};      // Rate of prediction model updates
        float exploration_bonus_scale{1.0f};       // Scale factor for exploration rewards
        float familiarity_decay_rate{0.01f};       // Rate of familiarity decay over time
        float complexity_weight{0.5f};             // Weight of complexity in novelty calculation
        float surprise_sensitivity{2.0f};          // Sensitivity to surprising events
        bool enable_prediction_learning{true};     // Whether to learn predictions
        bool enable_exploration_bonus{true};       // Whether to provide exploration bonuses
    };
    
private:
    Config config_;
    mutable std::mutex buffer_mutex_;
    
    // Experience buffer for novelty comparison
    std::deque<std::vector<float>> experience_buffer_;
    
    // Prediction model (simple running averages)
    std::vector<float> prediction_model_;
    std::vector<float> prediction_variance_;
    
    // Statistics and tracking
    std::atomic<std::uint64_t> total_experiences_{0};
    std::atomic<std::uint64_t> novel_experiences_{0};
    std::atomic<std::uint64_t> familiar_experiences_{0};
    std::atomic<float> average_novelty_{0.0f};
    std::atomic<float> average_exploration_bonus_{0.0f};
    
    // Time tracking for decay
    std::uint64_t last_update_time_ms_{0};
    
public:
    /**
     * @brief Constructor with configuration
     * @param config Novelty detection configuration
     */
    explicit NoveltyBias(const Config& config);
    
    /**
     * @brief Destructor
     */
    ~NoveltyBias() = default;
    
    // Core Novelty Detection
    
    /**
     * @brief Calculate novelty metrics for input
     * @param input Input vector to analyze
     * @return Comprehensive novelty metrics
     */
    NoveltyMetrics calculateNovelty(const std::vector<float>& input);
    
    /**
     * @brief Update experience buffer with new experience
     * @param experience New experience to add to buffer
     */
    void updateExperienceBuffer(const std::vector<float>& experience);
    
    /**
     * @brief Compute exploration bonus based on novelty metrics
     * @param metrics Novelty metrics to base bonus on
     * @return Exploration bonus value
     */
    float computeExplorationBonus(const NoveltyMetrics& metrics);
    
    /**
     * @brief Check if input is considered novel
     * @param input Input to check for novelty
     * @param threshold Optional custom threshold (uses config default if -1)
     * @return True if input is novel
     */
    bool isNovel(const std::vector<float>& input, float threshold = -1.0f);
    
    /**
     * @brief Update prediction model with new data
     * @param input Input that led to outcome
     * @param actual_outcome Actual outcome observed
     */
    void updatePredictionModel(const std::vector<float>& input, 
                              const std::vector<float>& actual_outcome);
    
    /**
     * @brief Get prediction for given input
     * @param input Input to predict outcome for
     * @return Predicted outcome
     */
    std::vector<float> getPrediction(const std::vector<float>& input) const;
    
    /**
     * @brief Calculate prediction error between predicted and actual
     * @param predicted Predicted values
     * @param actual Actual values
     * @return Prediction error magnitude
     */
    float calculatePredictionError(const std::vector<float>& predicted,
                                  const std::vector<float>& actual) const;
    
    /**
     * @brief Calculate information gain from exploring input
     * @param input Input to calculate information gain for
     * @return Expected information gain
     */
    float calculateInformationGain(const std::vector<float>& input) const;
    
    /**
     * @brief Calculate surprise level of input
     * @param input Input to assess surprise for
     * @return Surprise level
     */
    float calculateSurpriseLevel(const std::vector<float>& input) const;
    
    /**
     * @brief Calculate familiarity score of input
     * @param input Input to assess familiarity for
     * @return Familiarity score (0 = novel, 1 = very familiar)
     */
    float calculateFamiliarityScore(const std::vector<float>& input) const;
    
    /**
     * @brief Calculate complexity score of input
     * @param input Input to assess complexity for
     * @return Complexity score
     */
    float calculateComplexityScore(const std::vector<float>& input) const;
    
    // Time and Decay Management
    
    /**
     * @brief Apply temporal decay to stored experiences
     * @param delta_time_ms Time elapsed since last decay
     */
    void applyTemporalDecay(std::uint64_t delta_time_ms);
    
    /**
     * @brief Update internal time tracking
     * @param current_time_ms Current time in milliseconds
     */
    void updateTime(std::uint64_t current_time_ms);
    
    // Statistics and Configuration
    
    /**
     * @brief Statistics about novelty detection performance
     */
    struct Statistics {
        std::uint64_t total_experiences{0};
        std::uint64_t novel_experiences{0};
        std::uint64_t familiar_experiences{0};
        float novelty_rate{0.0f};
        float average_novelty{0.0f};
        float average_exploration_bonus{0.0f};
        size_t experience_buffer_size{0};
        size_t prediction_model_size{0};
        bool prediction_learning_active{false};
        bool exploration_bonus_active{false};
    };
    
    /**
     * @brief Get current statistics
     * @return Current novelty detection statistics
     */
    Statistics getStatistics() const;
    
    /**
     * @brief Get current novelty threshold
     * @return Current threshold value
     */
    float getNoveltyThreshold() const;

    /**
     * @brief Set novelty threshold
     * @param threshold New threshold value
     */
    void setNoveltyThreshold(float threshold);
    
    /**
     * @brief Get current configuration
     * @return Copy of current configuration
     */
    Config getConfig() const;

    /**
     * @brief Update configuration
     * @param new_config New configuration to apply
     * 
     * Note: Some configuration changes may require clearing
     * existing data structures for consistency.
     */
    void setConfig(const Config& new_config);
    
    /**
     * @brief Clear all stored experiences and reset state
     * 
     * Useful for starting fresh or when switching contexts
     * where previous experiences are no longer relevant.
     */
    void clear();
    
    /**
     * @brief Check if the novelty bias system is operational
     * @return True if system is ready for use
     */
    bool isOperational() const;

private:
    // Private helper methods
    
    /**
     * @brief Initialize prediction model with given input size
     * @param input_size Size of input vectors
     */
    void initializePredictionModel(size_t input_size);
    
    /**
     * @brief Calculate cosine similarity between two vectors
     * @param vec_a First vector
     * @param vec_b Second vector
     * @return Cosine similarity (-1 to 1)
     */
    float calculateCosineSimilarity(const std::vector<float>& vec_a,
                                   const std::vector<float>& vec_b) const;
    
    /**
     * @brief Calculate Euclidean distance between two vectors
     * @param vec_a First vector
     * @param vec_b Second vector
     * @return Euclidean distance
     */
    float calculateEuclideanDistance(const std::vector<float>& vec_a,
                                    const std::vector<float>& vec_b) const;
    
    /**
     * @brief Calculate entropy of input vector
     * @param input Input vector
     * @return Entropy value
     */
    float calculateEntropy(const std::vector<float>& input) const;
    
    /**
     * @brief Find most similar experience in buffer
     * @param input Input to find similarity for
     * @return Index of most similar experience (-1 if none)
     */
    int findMostSimilarExperience(const std::vector<float>& input) const;
    
    /**
     * @brief Update internal statistics with new metrics
     * @param metrics Novelty metrics to incorporate
     */
    void updateStatistics(const NoveltyMetrics& metrics);
    
    /**
     * @brief Validate input vector
     * @param input Input to validate
     * @return True if input is valid
     */
    bool validateInput(const std::vector<float>& input) const;
    
    // Thread-unsafe versions for internal use (caller must hold lock)
    float calculateFamiliarityScoreUnsafe(const std::vector<float>& input) const;
    float calculateSurpriseLevelUnsafe(const std::vector<float>& input) const;
    float calculateInformationGainUnsafe(const std::vector<float>& input) const;
    std::vector<float> getPredictionUnsafe(const std::vector<float>& input) const;
    void updateExperienceBufferUnsafe(const std::vector<float>& experience);
    void updateStatisticsUnsafe(const NoveltyMetrics& metrics);
};

} // namespace Biases
} // namespace NeuroForge