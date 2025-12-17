#pragma once

#include "Types.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>

namespace NeuroForge {
    namespace Core {

        // Forward declarations
        class HypergraphBrain;
        class SubstrateTaskGenerator;
        class AutonomousLearningCycle;
        class IntrinsicMotivationSystem;

        /**
         * @brief Comprehensive M7 autonomy metrics and KPI system
         * 
         * Provides detailed measurement and assessment of autonomous learning
         * performance, scaffold elimination progress, and system autonomy levels.
         */
        class M7AutonomyMetrics {
        public:
            /**
             * @brief Configuration for autonomy metrics collection
             */
            struct Config {
                std::uint32_t measurement_interval_ms{1000};    ///< Interval between measurements
                std::uint32_t history_size{1000};              ///< Size of metrics history
                float autonomy_threshold{0.9f};                ///< Threshold for full autonomy
                float performance_threshold{0.8f};             ///< Minimum performance threshold
                bool enable_detailed_logging{true};            ///< Enable detailed metric logging
                bool enable_real_time_assessment{true};        ///< Enable real-time assessment
            };

            /**
             * @brief Core autonomy KPIs
             */
            struct AutonomyKPIs {
                // Primary autonomy metrics
                float overall_autonomy_level{0.0f};            ///< Overall autonomy [0.0, 1.0]
                float task_generation_autonomy{0.0f};          ///< Task generation autonomy
                float learning_autonomy{0.0f};                 ///< Learning process autonomy
                float decision_making_autonomy{0.0f};          ///< Decision making autonomy
                float goal_setting_autonomy{0.0f};             ///< Goal setting autonomy
                
                // Scaffold elimination metrics
                float scaffold_elimination_progress{0.0f};     ///< Progress eliminating scaffolds
                float external_dependency_ratio{0.0f};         ///< Ratio of external dependencies
                float self_sufficiency_level{0.0f};           ///< Self-sufficiency level
                
                // Performance maintenance metrics
                float performance_during_transition{0.0f};     ///< Performance during autonomy transition
                float learning_effectiveness{0.0f};           ///< Learning effectiveness
                float adaptation_capability{0.0f};            ///< Adaptation to new situations
                
                // Intrinsic motivation metrics
                float intrinsic_motivation_dominance{0.0f};    ///< Intrinsic vs external motivation
                float curiosity_driven_behavior{0.0f};        ///< Curiosity-driven behavior ratio
                float self_directed_exploration{0.0f};        ///< Self-directed exploration level
                
                std::chrono::steady_clock::time_point timestamp; ///< Measurement timestamp
            };

            /**
             * @brief Detailed performance metrics
             */
            struct PerformanceMetrics {
                // Learning performance
                float learning_rate_stability{0.0f};          ///< Stability of learning rate
                float convergence_speed{0.0f};                ///< Speed of convergence
                float generalization_ability{0.0f};           ///< Generalization to new tasks
                float retention_capability{0.0f};             ///< Knowledge retention
                
                // Task execution performance
                float task_completion_rate{0.0f};             ///< Rate of task completion
                float task_success_rate{0.0f};                ///< Success rate of tasks
                float average_task_quality{0.0f};             ///< Average quality of task execution
                float resource_utilization_efficiency{0.0f};  ///< Resource utilization efficiency
                
                // System stability metrics
                float system_stability{0.0f};                 ///< Overall system stability
                float error_recovery_capability{0.0f};        ///< Error recovery capability
                float robustness_to_perturbations{0.0f};      ///< Robustness to external changes
                
                std::chrono::steady_clock::time_point timestamp; ///< Measurement timestamp
            };

            /**
             * @brief Behavioral assessment metrics
             */
            struct BehavioralMetrics {
                // Autonomous behavior patterns
                float self_initiated_action_ratio{0.0f};      ///< Ratio of self-initiated actions
                float proactive_behavior_level{0.0f};         ///< Level of proactive behavior
                float reactive_behavior_level{0.0f};          ///< Level of reactive behavior
                float exploratory_behavior_ratio{0.0f};       ///< Ratio of exploratory behavior
                
                // Decision making patterns
                float decision_confidence{0.0f};              ///< Confidence in decisions
                float decision_consistency{0.0f};             ///< Consistency of decisions
                float strategic_thinking_level{0.0f};         ///< Level of strategic thinking
                
                // Learning behavior
                float active_learning_engagement{0.0f};       ///< Active learning engagement
                float knowledge_seeking_behavior{0.0f};       ///< Knowledge seeking behavior
                float skill_development_focus{0.0f};          ///< Focus on skill development
                
                std::chrono::steady_clock::time_point timestamp; ///< Measurement timestamp
            };

            /**
             * @brief Comprehensive assessment result
             */
            struct AssessmentResult {
                AutonomyKPIs autonomy_kpis;                    ///< Core autonomy KPIs
                PerformanceMetrics performance_metrics;        ///< Performance metrics
                BehavioralMetrics behavioral_metrics;          ///< Behavioral metrics
                
                float composite_autonomy_score{0.0f};          ///< Composite autonomy score
                float composite_performance_score{0.0f};       ///< Composite performance score
                float composite_behavioral_score{0.0f};        ///< Composite behavioral score
                float overall_m7_score{0.0f};                 ///< Overall M7 achievement score
                
                bool meets_m7_criteria{false};                ///< Whether M7 criteria are met
                std::vector<std::string> achievement_gaps;     ///< Areas needing improvement
                std::vector<std::string> strengths;           ///< System strengths
                
                std::chrono::steady_clock::time_point timestamp; ///< Assessment timestamp
            };

            /**
             * @brief Constructor
             */
            M7AutonomyMetrics(std::shared_ptr<HypergraphBrain> brain,
                            std::shared_ptr<SubstrateTaskGenerator> task_generator,
                            std::shared_ptr<AutonomousLearningCycle> learning_cycle,
                            std::shared_ptr<IntrinsicMotivationSystem> motivation_system,
                            const Config& config);

            /**
             * @brief Initialize the metrics system
             */
            bool initialize();

            /**
             * @brief Shutdown the metrics system
             */
            void shutdown();

            /**
             * @brief Update all metrics
             * @param delta_time Time elapsed since last update
             */
            void updateMetrics(float delta_time);

            /**
             * @brief Perform comprehensive M7 assessment
             * @return Complete assessment result
             */
            AssessmentResult performComprehensiveAssessment();

            /**
             * @brief Calculate autonomy KPIs
             * @return Current autonomy KPIs
             */
            AutonomyKPIs calculateAutonomyKPIs();

            /**
             * @brief Calculate performance metrics
             * @return Current performance metrics
             */
            PerformanceMetrics calculatePerformanceMetrics();

            /**
             * @brief Calculate behavioral metrics
             * @return Current behavioral metrics
             */
            BehavioralMetrics calculateBehavioralMetrics();

            /**
             * @brief Check if M7 criteria are met
             * @return True if M7 criteria are satisfied
             */
            bool checkM7Criteria();

            /**
             * @brief Get current autonomy level
             * @return Overall autonomy level [0.0, 1.0]
             */
            float getCurrentAutonomyLevel();

            /**
             * @brief Get performance during autonomy transition
             * @return Performance level during transition [0.0, 1.0]
             */
            float getPerformanceDuringTransition();

            /**
             * @brief Get intrinsic motivation dominance
             * @return Intrinsic motivation dominance [0.0, 1.0]
             */
            float getIntrinsicMotivationDominance();

            /**
             * @brief Generate detailed metrics report
             * @return Formatted metrics report
             */
            std::string generateMetricsReport();

            /**
             * @brief Export metrics to CSV
             * @param filename Output filename
             * @return True if export successful
             */
            bool exportMetricsToCSV(const std::string& filename);

            /**
             * @brief Get metrics history
             * @param count Number of recent assessments to return
             * @return Vector of assessment results
             */
            std::vector<AssessmentResult> getMetricsHistory(std::size_t count = 10);

            /**
             * @brief Get current configuration
             */
            const Config& getConfig() const noexcept { return config_; }

            /**
             * @brief Update configuration
             */
            void setConfig(const Config& config) { config_ = config; }

        private:
            std::shared_ptr<HypergraphBrain> brain_;
            std::shared_ptr<SubstrateTaskGenerator> task_generator_;
            std::shared_ptr<AutonomousLearningCycle> learning_cycle_;
            std::shared_ptr<IntrinsicMotivationSystem> motivation_system_;
            Config config_;
            
            std::atomic<bool> is_active_{false};
            mutable std::mutex metrics_mutex_;
            
            std::vector<AssessmentResult> assessment_history_;
            std::chrono::steady_clock::time_point last_update_time_;
            std::chrono::steady_clock::time_point start_time_;
            
            // Baseline measurements for comparison
            PerformanceMetrics baseline_performance_;
            float initial_autonomy_level_{0.0f};
            
            /**
             * @brief Calculate composite scores
             */
            void calculateCompositeScores(AssessmentResult& result);

            /**
             * @brief Identify achievement gaps
             */
            void identifyAchievementGaps(AssessmentResult& result);

            /**
             * @brief Identify system strengths
             */
            void identifySystemStrengths(AssessmentResult& result);

            /**
             * @brief Update assessment history
             */
            void updateAssessmentHistory(const AssessmentResult& result);

            /**
             * @brief Calculate trend metrics
             */
            float calculateTrendMetric(const std::vector<float>& values);

            /**
             * @brief Normalize metric to [0.0, 1.0] range
             */
            float normalizeMetric(float value, float min_val, float max_val);
        };

    } // namespace Core
} // namespace NeuroForge