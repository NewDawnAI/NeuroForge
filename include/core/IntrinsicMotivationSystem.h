#pragma once

#include "Types.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>

namespace NeuroForge {
    namespace Core {

        // Forward declarations
        class HypergraphBrain;
        class LearningSystem;

        /**
         * @brief Enhanced intrinsic motivation system for M7 substrate autonomy
         * 
         * Provides sophisticated uncertainty, prediction error, and curiosity signals
         * to drive autonomous learning without external rewards.
         */
        class IntrinsicMotivationSystem {
        public:
            /**
             * @brief Configuration for intrinsic motivation system
             */
            struct Config {
                float uncertainty_weight{0.4f};            ///< Weight for uncertainty component
                float prediction_error_weight{0.3f};       ///< Weight for prediction error component
                float novelty_weight{0.2f};                ///< Weight for novelty component
                float curiosity_weight{0.1f};              ///< Weight for curiosity component
                float motivation_decay_rate{0.95f};        ///< Decay rate for motivation signals
                float surprise_threshold{0.5f};            ///< Threshold for surprise detection
                float exploration_bonus{0.2f};             ///< Bonus for exploration behavior
                std::uint32_t prediction_window{10};       ///< Window for prediction error calculation
                std::uint32_t novelty_memory_size{100};    ///< Size of novelty detection memory
                bool enable_meta_learning{true};           ///< Enable meta-learning motivation
                bool enable_competence_motivation{true};   ///< Enable competence-based motivation
            };

            /**
             * @brief Intrinsic motivation components
             */
            struct MotivationComponents {
                float uncertainty{0.0f};                   ///< Uncertainty-based motivation
                float prediction_error{0.0f};              ///< Prediction error motivation
                float novelty{0.0f};                       ///< Novelty-seeking motivation
                float curiosity{0.0f};                     ///< Curiosity-driven motivation
                float competence{0.0f};                    ///< Competence-building motivation
                float exploration{0.0f};                   ///< Exploration motivation
                float meta_learning{0.0f};                 ///< Meta-learning motivation
                float composite{0.0f};                     ///< Composite motivation score
                std::chrono::steady_clock::time_point timestamp; ///< Calculation timestamp
            };

            /**
             * @brief Prediction tracking for error calculation
             */
            struct PredictionTracker {
                std::vector<float> predictions;             ///< Recent predictions
                std::vector<float> actual_outcomes;         ///< Actual outcomes
                std::vector<float> errors;                  ///< Prediction errors
                float average_error{0.0f};                 ///< Average prediction error
                float error_variance{0.0f};                ///< Prediction error variance
            };

            /**
             * @brief Novelty detection system
             */
            struct NoveltyDetector {
                std::vector<std::vector<float>> experience_memory; ///< Memory of past experiences
                std::vector<float> novelty_scores;          ///< Recent novelty scores
                float novelty_threshold{0.5f};             ///< Threshold for novelty detection
                std::size_t memory_capacity{100};          ///< Maximum memory capacity
            };

            /**
             * @brief Constructor
             */
            IntrinsicMotivationSystem(std::shared_ptr<HypergraphBrain> brain,
                                    const Config& config);

            /**
             * @brief Initialize the intrinsic motivation system
             */
            bool initialize();

            /**
             * @brief Shutdown the intrinsic motivation system
             */
            void shutdown();

            /**
             * @brief Update intrinsic motivation based on current substrate state
             * @param delta_time Time elapsed since last update
             * @return Updated motivation components
             */
            MotivationComponents updateMotivation(float delta_time);

            /**
             * @brief Calculate uncertainty-based motivation
             * @return Uncertainty motivation [0.0, 1.0]
             */
            float calculateUncertaintyMotivation();

            /**
             * @brief Calculate prediction error motivation
             * @param prediction Current prediction
             * @param actual_outcome Actual outcome
             * @return Prediction error motivation [0.0, 1.0]
             */
            float calculatePredictionErrorMotivation(float prediction, float actual_outcome);

            /**
             * @brief Calculate novelty-based motivation
             * @param experience Current experience vector
             * @return Novelty motivation [0.0, 1.0]
             */
            float calculateNoveltyMotivation(const std::vector<float>& experience);

            /**
             * @brief Calculate curiosity-driven motivation
             * @return Curiosity motivation [0.0, 1.0]
             */
            float calculateCuriosityMotivation();

            /**
             * @brief Calculate competence-based motivation
             * @return Competence motivation [0.0, 1.0]
             */
            float calculateCompetenceMotivation();

            /**
             * @brief Calculate exploration motivation
             * @return Exploration motivation [0.0, 1.0]
             */
            float calculateExplorationMotivation();

            /**
             * @brief Calculate meta-learning motivation
             * @return Meta-learning motivation [0.0, 1.0]
             */
            float calculateMetaLearningMotivation();

            /**
             * @brief Generate intrinsic reward signal
             * @param motivation_components Current motivation components
             * @return Intrinsic reward value
             */
            float generateIntrinsicReward(const MotivationComponents& motivation_components);

            /**
             * @brief Detect surprise events in substrate dynamics
             * @param current_state Current substrate state
             * @return Surprise level [0.0, 1.0]
             */
            float detectSurprise(const std::vector<float>& current_state);

            /**
             * @brief Update prediction tracker with new data
             * @param prediction Predicted value
             * @param actual_outcome Actual outcome
             */
            void updatePredictionTracker(float prediction, float actual_outcome);

            /**
             * @brief Update novelty detector with new experience
             * @param experience Experience vector
             * @return Novelty score [0.0, 1.0]
             */
            float updateNoveltyDetector(const std::vector<float>& experience);

            /**
             * @brief Get current motivation components
             */
            const MotivationComponents& getCurrentMotivation() const noexcept { 
                return current_motivation_; 
            }

            /**
             * @brief Get prediction tracker statistics
             */
            const PredictionTracker& getPredictionTracker() const noexcept { 
                return prediction_tracker_; 
            }

            /**
             * @brief Get novelty detector state
             */
            const NoveltyDetector& getNoveltyDetector() const noexcept { 
                return novelty_detector_; 
            }

            /**
             * @brief Get current configuration
             */
            const Config& getConfig() const noexcept { return config_; }

            /**
             * @brief Update configuration
             */
            void setConfig(const Config& config) { config_ = config; }

            /**
             * @brief Check if motivation system is active
             */
            bool isActive() const noexcept { return is_active_.load(); }

            /**
             * @brief Get motivation history for analysis
             */
            std::vector<MotivationComponents> getMotivationHistory(std::size_t count = 10) const;

        private:
            std::shared_ptr<HypergraphBrain> brain_;
            Config config_;
            MotivationComponents current_motivation_;
            PredictionTracker prediction_tracker_;
            NoveltyDetector novelty_detector_;
            
            std::atomic<bool> is_active_{false};
            mutable std::mutex motivation_mutex_;
            
            std::vector<MotivationComponents> motivation_history_;
            std::vector<std::vector<float>> state_history_;
            std::chrono::steady_clock::time_point last_update_time_;
            
            /**
             * @brief Calculate activation variance across regions
             */
            float calculateActivationVariance() const;

            /**
             * @brief Calculate learning rate dynamics
             */
            float calculateLearningRateDynamics() const;

            /**
             * @brief Calculate substrate complexity
             */
            float calculateSubstrateComplexity() const;

            /**
             * @brief Update motivation history
             */
            void updateMotivationHistory(const MotivationComponents& components);

            /**
             * @brief Apply motivation decay
             */
            void applyMotivationDecay(float delta_time);

            /**
             * @brief Normalize motivation components
             */
            void normalizeMotivationComponents(MotivationComponents& components);

            /**
             * @brief Calculate composite motivation score
             */
            float calculateCompositeMotivation(const MotivationComponents& components);
        };

    } // namespace Core
} // namespace NeuroForge