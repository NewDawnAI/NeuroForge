#pragma once

#include "Types.h"
#include "core/HypergraphBrain.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

namespace NeuroForge {
namespace Core {

/**
 * @brief Substrate-based Working Memory for Milestone 4
 * 
 * Moves working memory, binding, and sequencing operations into the neural substrate
 * instead of using external symbolic representations.
 */
class SubstrateWorkingMemory {
public:
    /**
     * @brief Configuration for substrate working memory
     */
    struct Config {
        std::size_t working_memory_regions = 4;     ///< Number of WM regions to create
        std::size_t neurons_per_region = 100;       ///< Neurons per WM region
        float binding_threshold = 0.6f;             ///< Threshold for binding detection
        float sequence_threshold = 0.5f;            ///< Threshold for sequence prediction
        float decay_rate = 0.95f;                   ///< Decay rate for WM maintenance
        float maintenance_current = 0.3f;           ///< Maintenance current strength
        std::size_t max_binding_capacity = 6;       ///< Maximum concurrent bindings
    };

    /**
     * @brief Substrate binding representation
     */
    struct SubstrateBinding {
        NeuroForge::RegionID role_region;           ///< Region encoding the role
        NeuroForge::RegionID filler_region;         ///< Region encoding the filler
        std::vector<NeuroForge::NeuronID> binding_neurons; ///< Neurons maintaining binding
        float strength = 0.0f;                     ///< Binding strength
        std::string role_label;                     ///< Human-readable role label
        std::string filler_label;                   ///< Human-readable filler label
    };

    /**
     * @brief Substrate sequence state
     */
    struct SubstrateSequence {
        std::vector<NeuroForge::RegionID> sequence_regions; ///< Regions for sequence elements
        std::vector<float> prediction_activations;          ///< Current prediction state
        std::string current_token;                          ///< Current sequence token
        std::string predicted_token;                        ///< Predicted next token
        float prediction_confidence = 0.0f;                ///< Prediction confidence
    };

    /**
     * @brief Constructor
     */
    SubstrateWorkingMemory(std::shared_ptr<HypergraphBrain> brain, const Config& config);

    /**
     * @brief Initialize the substrate working memory system
     */
    bool initialize();

    /**
     * @brief Shutdown the system
     */
    void shutdown();

    /**
     * @brief Process one working memory step
     */
    void processStep(float delta_time);

    /**
     * @brief Create a role-filler binding in the substrate
     */
    bool createBinding(const std::string& role, const std::string& filler, float strength);

    /**
     * @brief Update sequence processing with new token
     */
    void updateSequence(const std::string& token);

    /**
     * @brief Get current bindings from substrate
     */
    std::vector<SubstrateBinding> getCurrentBindings() const;

    /**
     * @brief Get sequence prediction from substrate
     */
    SubstrateSequence getSequencePrediction() const;

    /**
     * @brief Apply maintenance current to working memory regions
     */
    void applyMaintenance();

    /**
     * @brief Apply decay to working memory representations
     */
    void applyDecay(float delta_time);

    /**
     * @brief Get working memory statistics
     */
    struct Statistics {
        std::size_t active_bindings = 0;
        std::size_t total_regions = 0;
        float average_binding_strength = 0.0f;
        float sequence_prediction_accuracy = 0.0f;
        std::size_t maintenance_cycles = 0;
    };
    Statistics getStatistics() const;

    /**
     * @brief Update configuration
     */
    void updateConfig(const Config& config) { config_ = config; }

private:
    Config config_;
    std::shared_ptr<HypergraphBrain> brain_;

    // Working memory regions
    std::vector<NeuroForge::RegionID> wm_regions_;
    std::vector<NeuroForge::RegionID> binding_regions_;
    std::vector<NeuroForge::RegionID> sequence_regions_;

    // Current state
    std::vector<SubstrateBinding> active_bindings_;
    SubstrateSequence current_sequence_;

    // Statistics
    mutable std::mutex stats_mutex_;
    Statistics stats_;

    // Helper methods
    NeuroForge::RegionID createWorkingMemoryRegion(const std::string& name);
    bool establishBinding(NeuroForge::RegionID role_region, NeuroForge::RegionID filler_region, float strength);
    void updateBindingStrengths();
    void pruneWeakBindings();
    std::vector<float> extractRegionActivation(NeuroForge::RegionID region_id) const;
    void injectRegionActivation(NeuroForge::RegionID region_id, const std::vector<float>& pattern);
    void updateStatistics();
};

} // namespace Core
} // namespace NeuroForge