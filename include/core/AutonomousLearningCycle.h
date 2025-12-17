#pragma once

#include "Types.h"
#include "SubstrateTaskGenerator.h"
#include "LearningSystem.h"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace NeuroForge {
    namespace Core {

        // Forward declarations
        class HypergraphBrain;
        class AutonomousScheduler;

        /**
         * @brief Autonomous learning cycle manager for M7 completion
         * 
         * Eliminates external scaffolding by implementing fully autonomous
         * learning cycles driven entirely by substrate dynamics and intrinsic motivation.
         */
        class AutonomousLearningCycle {
        public:
            /**
             * @brief Configuration for autonomous learning cycles
             */
            struct Config {
                float external_dependency_threshold{0.1f};  ///< Maximum allowed external dependency
                float autonomy_target{0.95f};               ///< Target autonomy level [0.0, 1.0]
                bool eliminate_teacher_dependency{true};    ///< Remove teacher-driven learning
                bool eliminate_external_rewards{true};     ///< Remove external reward signals
                bool eliminate_scripted_scenarios{true};   ///< Remove scripted learning scenarios
                bool enable_self_curriculum{true};         ///< Enable self-generated curriculum
                bool enable_curiosity_driven_exploration{true}; ///< Enable curiosity-based exploration
                std::uint32_t autonomy_assessment_interval{100}; ///< Cycles between autonomy assessments
                float performance_maintenance_threshold{0.95f}; ///< Minimum performance to maintain
            };

            /**
             * @brief Autonomy metrics for tracking scaffold elimination
             */
            struct AutonomyMetrics {
                float current_autonomy_level{0.0f};         ///< Current autonomy [0.0, 1.0]
                float external_dependency_ratio{1.0f};     ///< Ratio of external dependencies
                float self_initiated_task_ratio{0.0f};     ///< Ratio of self-initiated tasks
                float intrinsic_motivation_dominance{0.0f}; ///< Intrinsic vs external motivation
                float scaffold_elimination_progress{0.0f}; ///< Progress toward full autonomy
                std::uint64_t autonomous_cycles_completed{0}; ///< Number of fully autonomous cycles
                std::uint64_t total_learning_cycles{0};    ///< Total learning cycles executed
                std::chrono::steady_clock::time_point last_assessment; ///< Last autonomy assessment
            };

            /**
             * @brief Types of external scaffolding to eliminate
             */
            enum class ScaffoldType {
                TeacherGuidance,        ///< External teacher supervision
                ExternalRewards,        ///< External reward signals
                ScriptedScenarios,      ///< Predefined learning scenarios
                HandCraftedGoals,       ///< Manually specified goals
                ExternalCurriculum,     ///< External curriculum design
                PerformanceEvaluation,  ///< External performance assessment
                TaskSpecification,      ///< External task definitions
                EnvironmentalSetup      ///< External environment configuration
            };

            /**
             * @brief Constructor
             */
            AutonomousLearningCycle(std::shared_ptr<HypergraphBrain> brain,
                                  std::shared_ptr<SubstrateTaskGenerator> task_generator,
                                  const Config& config);

            /**
             * @brief Initialize autonomous learning cycle system
             */
            bool initialize();

            /**
             * @brief Shutdown autonomous learning cycle system
             */
            void shutdown();

            /**
             * @brief Execute one autonomous learning cycle
             * @param delta_time Time elapsed since last cycle
             * @return True if cycle completed successfully
             */
            bool executeAutonomousCycle(float delta_time);

            /**
             * @brief Assess current autonomy level and eliminate scaffolds
             * @return Current autonomy level [0.0, 1.0]
             */
            float assessAndEliminateScaffolds();

            /**
             * @brief Eliminate specific type of external scaffolding
             * @param scaffold_type Type of scaffold to eliminate
             * @return True if elimination successful
             */
            bool eliminateScaffold(ScaffoldType scaffold_type);

            /**
             * @brief Generate self-directed learning curriculum
             * @return Number of curriculum items generated
             */
            std::size_t generateSelfCurriculum();

            /**
             * @brief Implement curiosity-driven exploration
             * @return Exploration effectiveness score [0.0, 1.0]
             */
            float implementCuriosityDrivenExploration();

            /**
             * @brief Replace external rewards with intrinsic motivation
             * @return True if replacement successful
             */
            bool replaceExternalRewardsWithIntrinsic();

            /**
             * @brief Eliminate teacher dependency and enable self-teaching
             * @return True if elimination successful
             */
            bool eliminateTeacherDependency();

            /**
             * @brief Create autonomous performance evaluation system
             * @return True if system created successfully
             */
            bool createAutonomousEvaluationSystem();

            /**
             * @brief Monitor performance during scaffold elimination
             * @return Performance maintenance ratio [0.0, 1.0]
             */
            float monitorPerformanceDuringElimination();

            /**
             * @brief Calculate intrinsic motivation dominance
             * @return Intrinsic motivation ratio [0.0, 1.0]
             */
            float calculateIntrinsicMotivationDominance() const;

            /**
             * @brief Calculate self-initiated task ratio
             * @return Self-initiated task ratio [0.0, 1.0]
             */
            float calculateSelfInitiatedTaskRatio() const;

            /**
             * @brief Get current autonomy metrics
             */
            const AutonomyMetrics& getAutonomyMetrics() const noexcept { return metrics_; }

            /**
             * @brief Get current configuration
             */
            const Config& getConfig() const noexcept { return config_; }

            /**
             * @brief Update configuration
             */
            void setConfig(const Config& config) { config_ = config; }

            /**
             * @brief Check if full autonomy has been achieved
             * @return True if system is fully autonomous
             */
            bool isFullyAutonomous() const;

            /**
             * @brief Get scaffold elimination progress
             * @return Progress toward full autonomy [0.0, 1.0]
             */
            float getScaffoldEliminationProgress() const;

        private:
            std::shared_ptr<HypergraphBrain> brain_;
            std::shared_ptr<SubstrateTaskGenerator> task_generator_;
            Config config_;
            AutonomyMetrics metrics_;
            
            std::atomic<bool> is_active_{false};
            std::atomic<std::uint64_t> cycle_count_{0};
            mutable std::mutex metrics_mutex_;
            
            std::unordered_map<ScaffoldType, bool> scaffold_elimination_status_;
            std::vector<std::string> self_generated_curriculum_;
            std::chrono::steady_clock::time_point last_assessment_time_;
            
            // Performance tracking during scaffold elimination
            std::vector<float> performance_history_;
            float baseline_performance_{0.0f};
            
            /**
             * @brief Initialize scaffold elimination tracking
             */
            void initializeScaffoldTracking();

            /**
             * @brief Update autonomy metrics
             */
            void updateAutonomyMetrics();

            /**
             * @brief Validate performance maintenance during elimination
             */
            bool validatePerformanceMaintenance() const;

            /**
             * @brief Generate intrinsic learning objectives
             */
            std::vector<std::string> generateIntrinsicObjectives();

            /**
             * @brief Implement self-assessment mechanisms
             */
            float implementSelfAssessment();

            /**
             * @brief Create adaptive learning strategies
             */
            bool createAdaptiveLearningStrategies();
        };

    } // namespace Core
} // namespace NeuroForge