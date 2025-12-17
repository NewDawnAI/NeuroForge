#pragma once

#include "Types.h"
#include "HypergraphBrain.h"
#include "SubstrateWorkingMemory.h"
#include "PhaseC.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

namespace NeuroForge {
namespace Biases { class SurvivalBias; }
namespace Core {

/**
 * @brief Substrate-driven Phase C implementation for Milestone 4
 * 
 * Moves Phase C reasoning/binding/planning into recurrent dynamics and learned structure
 * within the neural substrate instead of external symbolic computation.
 */
class SubstratePhaseC {
public:
    /**
     * @brief Configuration for substrate Phase C
     */
    struct Config {
        std::size_t binding_regions = 6;            ///< Number of binding regions (role/filler pairs)
        std::size_t sequence_regions = 4;           ///< Number of sequence memory regions
        std::size_t neurons_per_region = 64;        ///< Neurons per specialized region
        float binding_threshold = 0.7f;             ///< Threshold for binding detection
        float sequence_threshold = 0.6f;            ///< Threshold for sequence prediction
        float binding_coherence_min = 0.5f;         ///< Minimum coherence for binding assemblies
        float sequence_coherence_min = 0.4f;        ///< Minimum coherence for sequence assemblies
        float competition_strength = 0.8f;          ///< Strength of competitive dynamics
        float recurrent_strength = 0.5f;            ///< Strength of recurrent connections
        float goal_setting_strength = 0.6f;         ///< Strength of goal-setting signals
        std::size_t max_assemblies = 10;            ///< Maximum concurrent assemblies
        float hazard_coherence_weight = 0.2f;       ///< Weight to down-modulate coherence under SurvivalBias
        bool emit_survival_rewards = false;         ///< Emit shaped reward per step using SurvivalBias metrics
        float survival_reward_scale = 1.0f;         ///< Scale factor for shaped reward magnitude
        float prune_coherence_threshold = 0.3f;     ///< Coherence threshold for pruning stale assemblies
    };

    /**
     * @brief Substrate assembly representation
     */
    struct SubstrateAssembly {
        std::vector<NeuroForge::NeuronID> neurons;   ///< Neurons participating in assembly
        std::vector<float> activation_pattern;       ///< Current activation pattern
        float coherence_score = 0.0f;               ///< Assembly coherence measure
        std::string symbol;                          ///< Associated symbol
        std::chrono::steady_clock::time_point last_active; ///< Last activation time
    };

    /**
     * @brief Substrate goal state
     */
    struct SubstrateGoal {
        std::string task_type;                       ///< "binding" or "sequence"
        std::vector<NeuroForge::RegionID> target_regions; ///< Regions to activate
        std::vector<float> target_pattern;           ///< Target activation pattern
        float priority = 1.0f;                      ///< Goal priority
        bool active = false;                         ///< Whether goal is currently active
    };

    /**
     * @brief Constructor
     */
    SubstratePhaseC(std::shared_ptr<HypergraphBrain> brain, 
                    std::shared_ptr<SubstrateWorkingMemory> working_memory,
                    const Config& config);

    /**
     * @brief Initialize substrate Phase C
     */
    bool initialize();

    /**
     * @brief Shutdown substrate Phase C
     */
    void shutdown();

    /**
     * @brief Set goal for substrate to achieve (replaces external computation)
     */
    void setGoal(const std::string& task_type, const std::map<std::string, std::string>& parameters);

    /**
     * @brief Process one step of substrate-driven Phase C behavior
     */
    void processStep(int step, float delta_time);

    /**
     * @brief Get current binding results from substrate activity
     */
    std::vector<BindingRow> getBindingResults(int step) const;

    /**
     * @brief Get current sequence results from substrate activity
     */
    SequenceRow getSequenceResult(int step) const;

    /**
     * @brief Get current assemblies detected in substrate
     */
    std::vector<SubstrateAssembly> getCurrentAssemblies() const;

    /**
     * @brief Get substrate statistics
     */
    struct Statistics {
        std::size_t assemblies_formed = 0;
        std::size_t bindings_created = 0;
        std::size_t sequences_predicted = 0;
        std::size_t goals_achieved = 0;
        float average_coherence = 0.0f;
        float binding_accuracy = 0.0f;
        float sequence_accuracy = 0.0f;
    };
    Statistics getStatistics() const { return stats_; }

    /**
     * @brief Update configuration
     */
    void updateConfig(const Config& config) { config_ = config; }

    // Targeted setters to avoid overwriting unrelated configuration fields
    void setMaxAssemblies(std::size_t m) noexcept { config_.max_assemblies = m; }
    void setHazardCoherenceWeight(float w) noexcept { config_.hazard_coherence_weight = w; }
    void setEmitSurvivalRewards(bool e) noexcept { config_.emit_survival_rewards = e; }
    void setSurvivalRewardScale(float s) noexcept { config_.survival_reward_scale = s; }

    /**
     * @brief Attach SurvivalBias to modulate assembly coherence under risk
     */
    void setSurvivalBias(std::shared_ptr<NeuroForge::Biases::SurvivalBias> bias) { survival_bias_ = std::move(bias); }
    void setJsonSink(std::function<void(const std::string&)> sink) { json_sink_ = std::move(sink); }

private:
    // Core substrate operations
    void initializeRegions();
    void setupRecurrentConnections();
    void processGoalSetting(float delta_time);
    void updateAssemblyDynamics(float delta_time);
    void detectBindings(int step);
    void predictSequences(int step);
    void updateCompetitiveDynamics();
    
    // Assembly management
    std::vector<SubstrateAssembly> detectActiveAssemblies() const;
    void updateAssemblyCoherence();
    void pruneStaleAssemblies();
    
    // Goal processing
    void activateGoalRegions(const SubstrateGoal& goal);
    void injectGoalSignals(const SubstrateGoal& goal);
    bool isGoalAchieved(const SubstrateGoal& goal) const;
    
    // Utility functions
    float calculateCoherence(const std::vector<NeuroForge::NeuronID>& neurons) const;
    std::vector<float> extractActivationPattern(const std::vector<NeuroForge::NeuronID>& neurons) const;
    void updateStatistics();
    void emitSurvivalReward();

private:
    Config config_;
    std::shared_ptr<HypergraphBrain> brain_;
    std::shared_ptr<SubstrateWorkingMemory> working_memory_;
    
    // Specialized regions for Phase C tasks
    std::vector<NeuroForge::RegionID> binding_regions_;
    std::vector<NeuroForge::RegionID> sequence_regions_;
    NeuroForge::RegionID competition_region_;
    NeuroForge::RegionID goal_region_;
    
    // Assembly tracking
    mutable std::mutex assemblies_mutex_;
    std::vector<SubstrateAssembly> current_assemblies_;
    std::unordered_map<std::string, std::size_t> assembly_lookup_;
    
    // Goal management
    mutable std::mutex goals_mutex_;
    std::vector<SubstrateGoal> active_goals_;
    SubstrateGoal current_goal_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    Statistics stats_;
    
    // State tracking
    std::atomic<bool> initialized_{false};
    std::atomic<bool> processing_{false};
    int current_step_ = 0;
    
    // Task-specific state
    std::vector<std::string> colors_{"red", "green", "blue"};
    std::vector<std::string> shapes_{"square", "circle", "triangle"};
    std::vector<std::string> seq_tokens_{"A", "B", "C", "D"};
    
    // Binding/sequence history for accuracy calculation
    std::vector<BindingRow> recent_bindings_;
    std::vector<SequenceRow> recent_sequences_;
    std::size_t max_history_size_ = 100;

    // Bias integration
    std::shared_ptr<NeuroForge::Biases::SurvivalBias> survival_bias_;
    std::function<void(const std::string&)> json_sink_;
};

} // namespace Core
} // namespace NeuroForge
