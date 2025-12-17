#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <numeric>

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
     * @param input Input vector to check
     * @param threshold Optional custom threshold
     * @return True if input is novel
     */
    bool isNovel(const std::vector<float>& input, float threshold = -1.0f);
    
    // Prediction Model Management
    
    /**
     * @brief Update prediction model with new experience
     * @param input Input that was predicted
     * @param actual_outcome Actual outcome that occurred
     */
    void updatePredictionModel(const std::vector<float>& input, 
                              const std::vector<float>& actual_outcome);
    
    /**
     * @brief Get prediction for given input
     * @param input Input to predict outcome for
     * @return Predicted outcome vector
     */
    std::vector<float> getPrediction(const std::vector<float>& input) const;
    
    /**
     * @brief Calculate prediction error between expected and actual
     * @param predicted Predicted outcome
     * @param actual Actual outcome
     * @return Prediction error magnitude
     */
    float calculatePredictionError(const std::vector<float>& predicted,
                                  const std::vector<float>& actual) const;
    
    // Advanced Novelty Analysis
    
    /**
     * @brief Calculate information gain from exploring input
     * @param input Input to calculate information gain for
     * @return Expected information gain
     */
    float calculateInformationGain(const std::vector<float>& input) const;
    
    /**
     * @brief Calculate surprise level for unexpected input
     * @param input Input to analyze for surprise
     * @return Surprise level [0,1]
     */
    float calculateSurpriseLevel(const std::vector<float>& input) const;
    
    /**
     * @brief Calculate familiarity score based on experience buffer
     * @param input Input to check familiarity for
     * @return Familiarity score [0,1]
     */
    float calculateFamiliarityScore(const std::vector<float>& input) const;
    
    /**
     * @brief Calculate complexity score of input
     * @param input Input to analyze complexity
     * @return Complexity score [0,1]
     */
    float calculateComplexityScore(const std::vector<float>& input) const;
    
    // Temporal Processing
    
    /**
     * @brief Apply temporal decay to familiarity and predictions
     * @param delta_time_ms Time elapsed since last update
     */
    void applyTemporalDecay(std::uint64_t delta_time_ms);
    
    /**
     * @brief Update internal time tracking
     * @param current_time_ms Current system time in milliseconds
     */
    void updateTime(std::uint64_t current_time_ms);
    
    // Configuration and Statistics
    
    /**
     * @brief Novelty detection statistics
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
     * @brief Get comprehensive novelty detection statistics
     * @return Current statistics
     */
    Statistics getStatistics() const;
    
    /**
     * @brief Set novelty detection threshold
     * @param threshold New threshold value [0,1]
     */
    void setNoveltyThreshold(float threshold);
    
    /**
     * @brief Get current novelty threshold
     * @return Current threshold value
     */
    float getNoveltyThreshold() const { return config_.novelty_threshold; }
    
    /**
     * @brief Update configuration
     * @param new_config New configuration parameters
     */
    void setConfig(const Config& new_config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    Config getConfig() const { return config_; }
    
    /**
     * @brief Clear experience buffer and reset prediction model
     */
    void clear();
    
    /**
     * @brief Check if novelty detection is operational
     * @return True if system is ready for use
     */
    bool isOperational() const;
    
private:
    /**
     * @brief Initialize prediction model for given input size
     * @param input_size Size of input vectors
     */
    void initializePredictionModel(size_t input_size);
    
    /**
     * @brief Calculate cosine similarity between two vectors
     * @param vec_a First vector
     * @param vec_b Second vector
     * @return Cosine similarity [-1,1]
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
     * @param input Input vector to analyze
     * @return Entropy value
     */
    float calculateEntropy(const std::vector<float>& input) const;
    
    /**
     * @brief Find most similar experience in buffer
     * @param input Input to find similarity for
     * @return Index of most similar experience, or -1 if buffer empty
     */
    int findMostSimilarExperience(const std::vector<float>& input) const;
    
    /**
     * @brief Update running statistics
     * @param metrics Latest novelty metrics
     */
    void updateStatistics(const NoveltyMetrics& metrics);
    
    /**
     * @brief Validate input vector
     * @param input Input vector to validate
     * @return True if input is valid
     */
    bool validateInput(const std::vector<float>& input) const;
    
    // Unsafe versions (assume mutex is already held)
    float calculateFamiliarityScoreUnsafe(const std::vector<float>& input) const;
    float calculateSurpriseLevelUnsafe(const std::vector<float>& input) const;
    float calculateInformationGainUnsafe(const std::vector<float>& input) const;
    std::vector<float> getPredictionUnsafe(const std::vector<float>& input) const;
    void updateExperienceBufferUnsafe(const std::vector<float>& experience);
    void updateStatisticsUnsafe(const NoveltyMetrics& metrics);
};

} // namespace Biases
} // namespace NeuroForge