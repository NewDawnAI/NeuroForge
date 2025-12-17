#pragma once

#include "Types.h"
#include "core/Region.h"
#include "core/Neuron.h"
#include "core/Synapse.h"
#include "connectivity/ConnectivityManager.h"
#include "core/LearningSystem.h"
#include "core/MemoryDB.h"
#include "core/AutonomousScheduler.h"
#include "core/SubstrateTaskGenerator.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <queue>
#include <optional>
#include <random>

// Include generated Cap'n Proto schema types used in this public header
#ifdef NF_HAVE_CAPNP
#include "neuroforge.capnp.h"
#include "brainstate.capnp.h"
#endif

namespace NeuroForge {
namespace Core {

// Forward declaration to avoid heavy header coupling
class SelfModel;
/**
 * @brief Main hypergraph brain class that orchestrates all regions and global processing
 * 
 * The HypergraphBrain serves as the central coordinator for the entire neural system,
 * managing regions, global connectivity patterns, and brain-wide dynamics.
 * Designed to scale to billions of neurons across hundreds of specialized regions.
 */
class HypergraphBrain {
public:
    using RegionContainer = std::unordered_map<NeuroForge::RegionID, NeuroForge::RegionPtr>;
    using RegionNameMap = std::unordered_map<std::string, NeuroForge::RegionID>;
    using GlobalSynapseContainer = std::vector<NeuroForge::SynapsePtr>;
    using ProcessingCallback = std::function<void(const HypergraphBrain&, float)>;
    using RegionUpdateOrder = std::vector<NeuroForge::RegionID>;
    using ModalityMap = std::unordered_map<NeuroForge::Modality, NeuroForge::RegionID>;
    
    /**
     * @brief Brain processing modes
     */
    enum class ProcessingMode {
        Sequential,     // Process regions one by one
        Parallel,       // Process regions in parallel
        Hierarchical,   // Process in hierarchical order (sensory -> cognitive -> motor)
        Custom          // Use custom processing order
    };

    /**
     * @brief Substrate operation modes for M7 autonomy
     */
    enum class SubstrateMode {
        Off,            // Substrate processing disabled
        Mirror,         // Mirror external inputs to substrate
        Train,          // Training mode with substrate learning
        Native          // Full native substrate operation
    };

    /**
     * @brief Brain state enumeration
     */
    enum class BrainState {
        Uninitialized,  // Brain not yet initialized
        Initializing,   // Brain is being initialized
        Running,        // Brain is actively processing
        Paused,         // Brain processing is paused
        Resetting,      // Brain is being reset
        Shutdown        // Brain is shutting down
    };

    /**
     * @brief Global brain statistics
     */
    struct GlobalStatistics {
        std::size_t total_regions = 0;
        std::size_t total_neurons = 0;
        std::size_t total_synapses = 0;
        std::size_t active_regions = 0;
        std::size_t active_neurons = 0;
        float global_activation = 0.0f;
        float total_energy = 0.0f;
        std::size_t total_memory_usage = 0;
        std::chrono::milliseconds total_processing_time{0};
        std::uint64_t processing_cycles = 0;
        float processing_frequency = 0.0f; // Hz
    };

    /**
     * @brief Hardware resource information
     */
    struct HardwareInfo {
        std::size_t available_memory = 0;
        std::size_t used_memory = 0;
        std::uint32_t cpu_cores = 0;
        std::uint32_t active_threads = 0;
        bool gpu_available = false;
        std::string gpu_info;
        float cpu_usage = 0.0f;
        float memory_usage = 0.0f;
    };

    /**
     * @brief Hippocampal-like memory snapshot for fast plasticity (M6)
     */
    struct HippocampalSnapshot {
        std::uint64_t timestamp_ms{0};
        std::uint64_t processing_cycle{0};
        std::unordered_map<NeuroForge::SynapseID, float> synapse_weights;
        std::unordered_map<NeuroForge::NeuronID, float> neuron_activations;
        std::unordered_map<NeuroForge::RegionID, std::vector<float>> region_states;
        float global_activation{0.0f};
        std::string context_tag;
        bool significant{false};
        
        // Metadata for consolidation
        float consolidation_priority{0.0f};
        float priority{0.0f};  // Priority for consolidation and access
        std::uint32_t access_count{0};
        std::uint64_t last_access_ms{0};
    };

    /**
     * @brief Configuration for hippocampal-like snapshotting (M6)
     */
    struct HippocampalConfig {
        bool enabled{true};
        std::size_t max_snapshots{1000};
        float snapshot_threshold{0.1f};  // Minimum activation change to trigger snapshot
        std::uint64_t snapshot_interval_ms{100};  // Minimum time between snapshots
        float consolidation_threshold{0.8f};  // Priority threshold for long-term consolidation
        bool auto_consolidation{true};
        std::size_t consolidation_batch_size{50};
        std::size_t max_consolidations_per_call{10};  // Maximum consolidations per processing step
        float decay_rate{0.95f};  // Decay rate for snapshot priorities
        float significance_boost{1.2f};  // Boost factor for significant snapshots
        std::uint64_t max_age_ms{86400000};  // Maximum age for snapshots (24 hours in ms)
    };

private:
    // Core brain components
    RegionContainer regions_;
    RegionNameMap region_names_;
    GlobalSynapseContainer global_synapses_;
    std::unordered_set<const Synapse*> global_synapse_ptrs_;
    // Modality routing to regions (guarded by region_mutex_)
    ModalityMap modality_region_map_;
    
    // Connectivity management
    std::shared_ptr<NeuroForge::Connectivity::ConnectivityManager> connectivity_manager_;
    
    // Learning system
     std::unique_ptr<NeuroForge::Core::LearningSystem> learning_system_;
     std::atomic<bool> learning_enabled_;
     
     // Processing configuration
    ProcessingMode processing_mode_;
    RegionUpdateOrder custom_update_order_;
    std::atomic<BrainState> brain_state_;
    std::atomic<bool> is_processing_;
    
    // Timing and synchronization
    std::atomic<float> target_frequency_; // Target processing frequency in Hz
    std::chrono::steady_clock::time_point last_update_time_;
    std::atomic<float> actual_frequency_;
    
    // Threading and parallelization
    std::vector<std::thread> processing_threads_;
    std::atomic<std::uint32_t> active_thread_count_;
    mutable std::mutex brain_mutex_;
    mutable std::mutex region_mutex_;
    mutable std::mutex statistics_mutex_;
    mutable std::mutex callback_mutex_;
    mutable std::mutex rng_mutex_;
    std::mt19937 rng_{std::random_device{}()};
    
    // Statistics and monitoring
    mutable GlobalStatistics global_stats_;
    std::atomic<std::uint64_t> processing_cycles_;
    std::atomic<int> reward_lag_align_offset_{0};
    
    // Callbacks and events
    std::vector<ProcessingCallback> pre_processing_callbacks_;
    std::vector<ProcessingCallback> post_processing_callbacks_;
    
    // Hardware awareness
    HardwareInfo hardware_info_;
    std::atomic<bool> hardware_monitoring_enabled_;
    
public:
    // Toggle procedural connectivity (Virtual Synapses)
    void setProceduralConnectivity(bool enabled) { procedural_connectivity_enabled_ = enabled; }
    bool isProceduralConnectivityEnabled() const { return procedural_connectivity_enabled_; }

private:
    // Procedural connectivity mode for massive scale (avoids storing Synapse objects)
    bool procedural_connectivity_enabled_{false};

    // M6: Hippocampal-like snapshotting for fast plasticity memory path
    HippocampalConfig hippocampal_config_;
    std::vector<HippocampalSnapshot> hippocampal_snapshots_;
    mutable std::mutex hippocampal_mutex_;
    std::uint64_t last_snapshot_time_ms_{0};
    float last_global_activation_{0.0f};
    std::atomic<bool> hippocampal_enabled_{true};

    // Autonomous task scheduling system
    std::unique_ptr<AutonomousScheduler> autonomous_scheduler_;
    std::unique_ptr<SubstrateTaskGenerator> substrate_task_generator_;
    std::atomic<bool> autonomous_mode_enabled_{false};
    mutable std::mutex scheduler_mutex_;
    std::thread::id main_thread_id_;
    std::thread::id autonomous_thread_id_;

    // M7: Substrate mode and autonomous operation
    std::atomic<SubstrateMode> substrate_mode_{SubstrateMode::Off};
    std::atomic<float> curiosity_threshold_{0.3f};
    std::atomic<float> uncertainty_threshold_{0.4f};
    std::atomic<float> prediction_error_threshold_{0.5f};
    std::atomic<int> max_concurrent_tasks_{5};
    std::atomic<int> task_generation_interval_{1000};
    std::atomic<bool> eliminate_scaffolds_{false};
    std::atomic<bool> autonomy_metrics_enabled_{false};
    std::atomic<float> autonomy_target_{0.9f};
    std::atomic<float> motivation_decay_{0.95f};
    std::atomic<float> exploration_bonus_{0.2f};
    std::atomic<int> novelty_memory_size_{100};

    std::atomic<bool> selfnode_integration_enabled_{false};
    std::atomic<bool> pfc_integration_enabled_{false};
    std::atomic<bool> motor_cortex_integration_enabled_{false};
    
    // Unified Self System: cached read-only SelfModel (Step 2)
    std::unique_ptr<SelfModel> self_model_;

    // M6: Helper methods for hippocampal snapshotting
    /**
     * @brief Calculate current global activation level
     * @return Global activation value
     */
    float calculateGlobalActivation() const;

    /**
     * @brief Check if snapshot should be taken based on thresholds
     * @param current_activation Current global activation level
     * @return True if snapshot should be taken
     */
    bool shouldTakeSnapshot(float current_activation) const;

    /**
     * @brief Capture current brain state into a snapshot
     * @param snapshot Reference to snapshot to populate
     * @param context_tag Context tag for the snapshot
     */
    void captureCurrentState(HippocampalSnapshot& snapshot, const std::string& context_tag) const;

    /**
     * @brief Update snapshot priorities and decay old ones
     */
    void updateSnapshotPriorities();

    /**
     * @brief Select snapshots for consolidation based on priority
     * @param force_all Force selection of all snapshots
     * @return Indices of snapshots to consolidate
     */
    std::vector<std::size_t> selectSnapshotsForConsolidation(bool force_all) const;

public:
    /**
     * @brief Construct a new HypergraphBrain
     * 
     * @param connectivity_manager Shared pointer to connectivity manager
     * @param target_frequency Target processing frequency in Hz (default: 100 Hz)
     * @param processing_mode Initial processing mode
     */
    explicit HypergraphBrain(std::shared_ptr<NeuroForge::Connectivity::ConnectivityManager> connectivity_manager,
                          float target_frequency = 100.0f,
                          ProcessingMode processing_mode = ProcessingMode::Parallel);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~HypergraphBrain();

    void setRandomSeed(std::uint32_t seed);

    // Brain lifecycle management
    /**
     * @brief Initialize the brain system
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Start brain processing
     * @return True if started successfully
     */
    bool start();

    /**
     * @brief Pause brain processing
     */
    void pause();

    /**
     * @brief Resume brain processing
     */
    void resume();

    /**
     * @brief Stop brain processing
     */
    void stop();

    /**
     * @brief Reset brain to initial state
     */
    void reset();

    /**
     * @brief Shutdown brain system
     */
    void shutdown();

    // M6: Hippocampal-like snapshotting methods for fast plasticity memory path
    /**
     * @brief Configure hippocampal-like snapshotting system
     * @param config Configuration parameters for snapshotting
     */
    void configureHippocampalSnapshotting(const HippocampalConfig& config);

    /**
     * @brief Take a hippocampal-like snapshot of current brain state
     * @param context_tag Optional context tag for the snapshot
     * @param force_snapshot Force snapshot even if threshold not met
     * @return True if snapshot was taken
     */
    bool takeHippocampalSnapshot(const std::string& context_tag = "", bool force_snapshot = false);

    /**
     * @brief Consolidate hippocampal snapshots to long-term memory
     * @param force_all Force consolidation of all snapshots regardless of priority
     * @return Number of snapshots consolidated
     */
    std::size_t consolidateHippocampalSnapshots(bool force_all = false);

    /**
     * @brief Get hippocampal snapshot statistics
     */
    struct HippocampalStats {
        std::size_t total_snapshots{0};
        std::size_t significant_snapshots{0};
        std::size_t consolidated_snapshots{0};
        float average_priority{0.0f};
        std::uint64_t last_snapshot_time_ms{0};
        std::uint64_t last_consolidation_time_ms{0};
        std::size_t memory_usage_bytes{0};
    };
    HippocampalStats getHippocampalStats() const;

    /**
     * @brief Enable or disable hippocampal snapshotting
     * @param enabled True to enable, false to disable
     */
    void setHippocampalEnabled(bool enabled);

    /**
     * @brief Check if hippocampal snapshotting is enabled
     * @return True if enabled
     */
    bool isHippocampalEnabled() const;

    // Region management
    /**
     * @brief Add a region to the brain
     * @param region Shared pointer to the region
     * @return True if successfully added
     */
    bool addRegion(NeuroForge::RegionPtr region);

    /**
     * @brief Remove a region from the brain
     * @param region_id ID of the region to remove
     * @return True if successfully removed
     */
    bool removeRegion(NeuroForge::RegionID region_id);

    /**
     * @brief Get region by ID
     * @param region_id Region ID
     * @return Shared pointer to region, or nullptr if not found
     */
    NeuroForge::RegionPtr getRegion(NeuroForge::RegionID region_id) const;

    /**
     * @brief Get region by name
     * @param name Region name
     * @return Shared pointer to region, or nullptr if not found
     */
    NeuroForge::RegionPtr getRegion(const std::string& name) const;

    /**
     * @brief Get all regions
     * @return Const reference to region container
     */
    const RegionContainer& getRegions() const noexcept { return regions_; }

    /**
     * @brief Get regions map for direct access (used by LearningSystem to avoid deadlock)
     * @return Const reference to regions map
     */
    const RegionContainer& getRegionsMap() const noexcept { return regions_; }

    /**
     * @brief Get region mutex for controlled access (used by LearningSystem to avoid deadlock)
     * @return Reference to region mutex
     */
    std::mutex& getRegionMutex() const noexcept { return region_mutex_; }

    /**
     * @brief Create and add a new region
     * @param name Region name
     * @param type Region type
     * @param pattern Activation pattern
     * @return Shared pointer to created region
     */
    NeuroForge::RegionPtr createRegion(const std::string& name,
                                     Region::Type type = Region::Type::Custom,
                                     Region::ActivationPattern pattern = Region::ActivationPattern::Asynchronous);

    // Neural substrate API: modality routing and I/O
    /** Map a high-level modality to a target region ID */
    void mapModality(NeuroForge::Modality modality, NeuroForge::RegionID region_id);
    /** Get the region pointer currently associated with a modality (nullptr if unmapped or missing) */
    NeuroForge::RegionPtr getModalityRegion(NeuroForge::Modality modality) const;
    /** Feed an external feature vector into the region associated with the modality */
    void feedExternalPattern(NeuroForge::Modality modality, const std::vector<float>& pattern);
    /** Read out the current activity vector from the region associated with the modality */
    std::vector<float> readoutVector(NeuroForge::Modality modality) const;
    /** Apply a global neuromodulator level to the region associated with the modality */
    void applyNeuromodulator(NeuroForge::Modality modality, float level);
    
    // Global connectivity
    /**
     * @brief Connect two regions
     * @param source_region_id Source region ID
     * @param target_region_id Target region ID
     * @param connection_density Connection density (0.0 to 1.0)
     * @param weight_range Weight range for connections
     * @return Number of connections created
     */
    std::size_t connectRegions(NeuroForge::RegionID source_region_id,
                               NeuroForge::RegionID target_region_id,
                               float connection_density = 0.1f,
                               std::pair<float, float> weight_range = {0.1f, 0.9f});

    /**
     * @brief Connect specific neurons between regions
     * @param source_region_id Source region ID
     * @param target_region_id Target region ID
     * @param source_neuron_id Source neuron ID
     * @param target_neuron_id Target neuron ID
     * @param weight Synapse weight
     * @param type Synapse type
     * @return Shared pointer to created synapse
     */
    NeuroForge::SynapsePtr connectNeurons(NeuroForge::RegionID source_region_id,
                                         NeuroForge::RegionID target_region_id,
                                         NeuroForge::NeuronID source_neuron_id,
                                         NeuroForge::NeuronID target_neuron_id,
                                         NeuroForge::Weight weight = 0.5f,
                                         NeuroForge::SynapseType type = NeuroForge::SynapseType::Excitatory);

    // Overload: Connect specific neurons between regions with an explicit Synapse ID
    NeuroForge::SynapsePtr connectNeurons(NeuroForge::RegionID source_region_id,
                                         NeuroForge::RegionID target_region_id,
                                         NeuroForge::NeuronID source_neuron_id,
                                         NeuroForge::NeuronID target_neuron_id,
                                         NeuroForge::Weight weight,
                                         NeuroForge::SynapseType type,
                                         NeuroForge::SynapseID explicit_id);
    // Processing control
    /**
     * @brief Process one simulation step
     * @param delta_time Time step in seconds
     */
    void processStep(float delta_time);

    /**
     * @brief Set processing mode
     * @param mode New processing mode
     */
    void setProcessingMode(ProcessingMode mode) { processing_mode_ = mode; }

    /**
     * @brief Get current processing mode
     * @return Current processing mode
     */
    ProcessingMode getProcessingMode() const noexcept { return processing_mode_; }

    /**
     * @brief Set custom region update order
     * @param order Vector of region IDs in processing order
     */
    void setCustomUpdateOrder(const RegionUpdateOrder& order) { custom_update_order_ = order; }

    /**
     * @brief Set target processing frequency
     * @param frequency Target frequency in Hz
     */
    void setTargetFrequency(float frequency) { target_frequency_.store(frequency, std::memory_order_relaxed); }

    /**
     * @brief Get target processing frequency
     * @return Target frequency in Hz
     */
    float getTargetFrequency() const noexcept { return target_frequency_.load(std::memory_order_relaxed); }

    /**
     * @brief Get actual processing frequency
     * @return Actual frequency in Hz
     */
    float getActualFrequency() const noexcept { return actual_frequency_.load(std::memory_order_relaxed); }

    // State and status
    /**
     * @brief Get current brain state
     * @return Current brain state
     */
    BrainState getBrainState() const noexcept { return brain_state_.load(std::memory_order_relaxed); }

    /**
     * @brief Check if brain is currently processing
     * @return True if processing
     */
    bool isProcessing() const noexcept { return is_processing_.load(std::memory_order_relaxed); }

    /**
     * @brief Get brain state as string
     * @return String representation of brain state
     */
    std::string getBrainStateString() const;

    // Statistics and monitoring
    /**
     * @brief Get global brain statistics
     * @return Global statistics structure
     */
    GlobalStatistics getGlobalStatistics() const;

    /**
     * @brief Get hardware information
     * @return Hardware info structure
     */
    HardwareInfo getHardwareInfo() const;

    /**
     * @brief Enable or disable hardware monitoring
     * @param enabled Enable hardware monitoring
     */
    void setHardwareMonitoring(bool enabled) { hardware_monitoring_enabled_.store(enabled, std::memory_order_relaxed); }

    /**
     * @brief Get total memory usage
     * @return Memory usage in bytes
     */
    std::size_t getTotalMemoryUsage() const;

    /**
     * @brief Get processing cycles completed
     * @return Processing cycle count
     */
    std::uint64_t getProcessingCycles() const noexcept {
        return processing_cycles_.load(std::memory_order_relaxed);
    }

    // Callback management
    /**
     * @brief Add pre-processing callback
     * @param callback Callback function
     */
    void addPreProcessingCallback(ProcessingCallback callback) {
        pre_processing_callbacks_.push_back(std::move(callback));
    }

    /**
     * @brief Add post-processing callback
     * @param callback Callback function
     */
    void addPostProcessingCallback(ProcessingCallback callback) {
        post_processing_callbacks_.push_back(std::move(callback));
    }

    /**
     * @brief Clear all callbacks
     */
    void clearCallbacks() {
        pre_processing_callbacks_.clear();
        post_processing_callbacks_.clear();
    }

    // Learning system management
    /**
     * @brief Initialize learning system
     * @param config Learning configuration
     * @return True if initialization successful
     */
    bool initializeLearning(const NeuroForge::Core::LearningSystem::Config& config = {});

    /**
     * @brief Enable or disable learning
     * @param enabled Enable learning
     */
    void setLearningEnabled(bool enabled) { learning_enabled_.store(enabled, std::memory_order_relaxed); }

    /**
     * @brief Check if learning is enabled
     * @return True if learning is enabled
     */
    bool isLearningEnabled() const noexcept { return learning_enabled_.load(std::memory_order_relaxed); }

    /**
     * @brief Get learning system
     * @return Pointer to learning system, or nullptr if not initialized
     */
    NeuroForge::Core::LearningSystem* getLearningSystem() const noexcept { return learning_system_.get(); }

    /**
     * @brief Deliver an external reward signal to the learning system and log it
     * @param reward Reward value (task/novelty/mimicry shaped)
     * @param source Optional source label (e.g., "task", "mimicry", "external")
     * @param context_json Optional JSON context payload for MemoryDB logging
     */
    void deliverReward(double reward, const std::string& source = "", const std::string& context_json = "{}");

    /**
     * @brief Set lag alignment offset applied to reward log step for Phase C
     * @param offset Signed offset added to processing cycle when logging rewards
     */
    void setPhaseCLagAlign(int offset) { reward_lag_align_offset_.store(offset, std::memory_order_relaxed); }

    // ... Mimicry shaping bridge (forward to LearningSystem) =====
    /** Enable or disable mimicry shaping term in reward */
    void setMimicryEnabled(bool enabled);
    /** Set mimicry reward weight mu */
    void setMimicryWeight(float mu);
    /** Provide teacher embedding vector */
    void setTeacherVector(const std::vector<float>& teacher);
    /** Provide current student embedding vector */
    void setStudentEmbedding(const std::vector<float>& student);
    /** Enable internal Phase A mimicry usage inside LearningSystem shaping */
    void setMimicryInternal(bool enabled);
    /** Provide last Phase A attempt scores */
    void setMimicryAttemptScores(float similarity, float novelty, float total_reward, bool success);
    /** Retrieve last computed cosine similarity between teacher and student embeddings */
    float getLastMimicrySimilarity() const;

    /**
     * @brief Apply Hebbian learning to specific region
     * @param region_id Target region ID
     * @param learning_rate Learning rate override (-1 for default)
     */
    void applyHebbianLearning(NeuroForge::RegionID region_id, float learning_rate = -1.0f);

    /**
     * @brief Consolidate memories in specified regions
     * @param regions Vector of region IDs for consolidation
     */
    void consolidateMemories(const std::vector<NeuroForge::RegionID>& regions);

    /**
     * @brief Apply attention-modulated learning
     * @param attention_map Map of neuron IDs to attention weights
     * @param learning_boost Attention boost factor
     */
    void applyAttentionModulation(
        const std::unordered_map<NeuroForge::NeuronID, float>& attention_map,
        float learning_boost = 2.0f);

    void biasNeuronActivation(NeuroForge::NeuronID neuron_id, float influence_strength);

     /**
       * @brief Get learning statistics
       * @return Learning statistics if available
       */
      std::optional<NeuroForge::Core::LearningSystem::Statistics> getLearningStatistics() const;

      // Utility functions
    /**
     * @brief Get processing mode as string
     * @return String representation of processing mode
     */
    std::string getProcessingModeString() const;

    /**
     * @brief Export brain structure to JSON
     * @return JSON string representation
     */
    std::string exportToJson() const;
    // Cap'n Proto binary export/import (always available)
#ifdef NF_HAVE_CAPNP
    // Mapping helpers between internal enums and Cap'n Proto schema types
    NeuroForge::Schema::ProcessingMode mapProcessingMode(ProcessingMode mode) const;
    ProcessingMode unmapProcessingMode(NeuroForge::Schema::ProcessingMode mode) const;

    NeuroForge::Schema::BrainState mapBrainState(BrainState state) const;
    BrainState unmapBrainState(NeuroForge::Schema::BrainState state) const;

    NeuroForge::Schema::RegionType mapRegionType(Region::Type type) const;
    Region::Type unmapRegionType(NeuroForge::Schema::RegionType type) const;

    NeuroForge::Schema::ActivationPattern mapActivationPattern(Region::ActivationPattern pattern) const;
    Region::ActivationPattern unmapActivationPattern(NeuroForge::Schema::ActivationPattern pattern) const;

    NeuroForge::Schema::NeuronState mapNeuronState(Neuron::State state) const;
    Neuron::State unmapNeuronState(NeuroForge::Schema::NeuronState state) const;

    NeuroForge::Schema::SynapseType mapSynapseType(NeuroForge::SynapseType type) const;
    NeuroForge::SynapseType unmapSynapseType(NeuroForge::Schema::SynapseType type) const;

    NeuroForge::Schema::PlasticityRule mapPlasticityRule(Synapse::PlasticityRule rule) const;
    Synapse::PlasticityRule unmapPlasticityRule(NeuroForge::Schema::PlasticityRule rule) const;

    void serializeSynapse(NeuroForge::Schema::Synapse::Builder& builder, const SynapsePtr& synapse) const;
    SynapsePtr deserializeSynapse(
        const NeuroForge::Schema::Synapse::Reader& reader,
        const std::unordered_map<uint32_t, std::unordered_map<uint64_t, NeuronPtr>>& regionNeuronMap) const;

    // Cap'n Proto binary export/import
    bool exportToCapnp(std::vector<uint8_t>& outBuffer) const;
    bool importFromCapnp(const uint8_t* data, size_t size);
    bool importFromJson(const std::string& json_data);
#endif // NF_HAVE_CAPNP

#ifdef NF_HAVE_CAPNP
    // New BrainStateFile (wrapper) Cap'n Proto I/O
    bool exportToBrainStateCapnp(std::vector<uint8_t>& outBuffer) const;
    bool importFromBrainStateCapnp(const uint8_t* data, size_t size);
#endif // NF_HAVE_CAPNP

    // New: Phase 3 - Persistence API
    /**
     * @brief Save full checkpoint to a file (safe write via temp file rename)
     * @param filepath Target path for checkpoint file
     * @param pretty Pretty-print JSON (currently always pretty)
     * @return True on success
     */
    bool saveCheckpoint(const std::string& filepath, bool pretty = true) const;

    // Unified state persistence wrappers (Phase 3): prefer Cap'n Proto via extension
    bool saveState(const std::string& filepath) const { return saveCheckpoint(filepath, /*pretty=*/true); }

    /**
     * @brief Load full checkpoint from a file
     * @param filepath Source path for checkpoint file
     * @return True on success
     */
    bool loadCheckpoint(const std::string& filepath);

    // Unified state load wrapper (Phase 3)
    bool loadState(const std::string& filepath) { return loadCheckpoint(filepath); }

    /**
     * @brief Add an experience record to the in-memory experience buffer
     * @param tag Short descriptor (e.g., modality or scenario)
     * @param input Encoded input feature vector
     * @param output Observed output/response vector
     * @param significant Mark as significant episodic memory
     */
    void addExperience(const std::string& tag,
                       const std::vector<float>& input,
                       const std::vector<float>& output,
                       bool significant = false);

    /**
     * @brief Configure maximum number of experiences to retain in memory (ring buffer semantics)
     */
    void setExperienceCapacity(std::size_t capacity);

    /**
     * @brief Get current number of stored experiences
     */
    std::size_t getExperienceCount() const noexcept;

    /**
     * @brief Clear all experiences and episodes
     */
    void clearExperiences();

    // Enable verbose debug logs for MemoryDB propagation to regions
    void setMemoryPropagationDebug(bool enabled) { memdb_propagation_debug_ = enabled; }
    // Enable ANSI color for MemoryDB debug logs (when supported by terminal)
    void setMemoryDBColorize(bool enabled) { memdb_colorize_ = enabled; }

    void setMemoryDB(std::shared_ptr<NeuroForge::Core::MemoryDB> db, std::int64_t run_id);

    /**
     * @brief Start a new episode with the given name
     * @param name Episode name/identifier
     * @return Episode ID if successful, -1 if failed
     */
    std::int64_t startEpisode(const std::string& name);

    /**
     * @brief End the current episode
     * @param episode_id Episode ID to end
     * @return True if successful
     */
    bool endEpisode(std::int64_t episode_id);

    /**
     * @brief Log a reward signal to the MemoryDB if configured
     * @param reward Reward value (e.g., task reward or novelty signal)
     * @param source Optional source label (e.g., "novelty", "task", "external")
     * @param context_json Optional JSON blob providing context
     */
    void logReward(double reward, const std::string& source = "", const std::string& context_json = "{}");

    /**
     * @brief Log a self-model snapshot to the MemoryDB if configured
     * @param state_json JSON-encoded self state
     * @param confidence Confidence score in [0,1] (or any domain-specific range)
     */
    void logSelfModel(const std::string& state_json, double confidence);

    void logSubstrateState(const std::string& state_type,
                           const std::string& region_id,
                           const std::string& serialized_data);

    // Allow external observer to receive spike events (for UI overlays)
    void setSpikeObserver(const std::function<void(NeuroForge::NeuronID, NeuroForge::TimePoint)>& observer) { spike_observer_ = observer; }

    // Autonomous task scheduling system
    /**
     * @brief Initialize the autonomous scheduler
     * @param config Scheduler configuration
     * @return True if initialization successful
     */
    bool initializeAutonomousScheduler(const AutonomousScheduler::Config& config = {});

    /**
     * @brief Enable or disable autonomous mode
     * @param enabled Enable autonomous task execution
     */
    void setAutonomousModeEnabled(bool enabled);

    /**
     * @brief Check if autonomous mode is enabled
     * @return True if autonomous mode is enabled
     */
    bool isAutonomousModeEnabled() const noexcept { return autonomous_mode_enabled_.load(std::memory_order_relaxed); }

    /**
     * @brief Add a task to the autonomous scheduler
     * @param task Shared pointer to the task
     * @return True if task was successfully added
     */
    bool addAutonomousTask(std::shared_ptr<AutonomousTask> task);

    /**
     * @brief Execute one autonomous processing cycle
     * @param delta_time Time step in seconds
     * @return True if any tasks were executed
     */
    bool executeAutonomousCycle(float delta_time);

    /**
     * @brief Get autonomous scheduler statistics
     * @return Scheduler statistics if available
     */
    std::optional<AutonomousScheduler::Statistics> getAutonomousStatistics() const;

    /**
     * @brief Get the autonomous scheduler instance
     * @return Pointer to scheduler, or nullptr if not initialized
     */
    AutonomousScheduler* getAutonomousScheduler() const noexcept { return autonomous_scheduler_.get(); }

    /**
     * @brief Get the substrate task generator instance
     * @return Pointer to task generator, or nullptr if not initialized
     */
    SubstrateTaskGenerator* getSubstrateTaskGenerator() const noexcept { return substrate_task_generator_.get(); }

    /**
     * @brief Initialize substrate-driven task generation for M7 autonomy
     * @param config Configuration for substrate task generation
     * @return True if initialization successful
     */
    bool initializeSubstrateTaskGeneration(const SubstrateTaskGenerator::Config& config);

    /**
     * @brief Enable or disable substrate-driven task generation
     * @param enabled Enable substrate task generation
     */
    void setSubstrateTaskGenerationEnabled(bool enabled);

    /**
     * @brief Check if substrate task generation is enabled
     * @return True if substrate task generation is active
     */
    bool isSubstrateTaskGenerationEnabled() const;

    /**
     * @brief Run the autonomous loop (blocking call)
     * This implements the main autonomous agent loop with goal selection,
     * planning, execution, and reflection phases
     * @param max_iterations Maximum iterations to run (0 for infinite)
     * @param target_frequency Target frequency in Hz
     */
    void runAutonomousLoop(std::size_t max_iterations = 0, float target_frequency = 10.0f);

    void setSelfNodeIntegrationEnabled(bool enabled);
    bool isSelfNodeIntegrationEnabled() const noexcept { return selfnode_integration_enabled_.load(std::memory_order_relaxed); }
    void setPrefrontalCortexIntegrationEnabled(bool enabled);
    bool isPrefrontalCortexIntegrationEnabled() const noexcept { return pfc_integration_enabled_.load(std::memory_order_relaxed); }
    void setMotorCortexIntegrationEnabled(bool enabled);
    bool isMotorCortexIntegrationEnabled() const noexcept { return motor_cortex_integration_enabled_.load(std::memory_order_relaxed); }

    // M7: Substrate mode and autonomous operation methods
    /**
     * @brief Set substrate operation mode
     * @param mode Substrate mode (Off, Mirror, Train, Native)
     */
    void setSubstrateMode(SubstrateMode mode);

    /**
     * @brief Get current substrate mode
     * @return Current substrate mode
     */
    SubstrateMode getSubstrateMode() const noexcept { return substrate_mode_.load(std::memory_order_relaxed); }

    /**
     * @brief Set curiosity threshold for autonomous task generation
     * @param threshold Curiosity threshold [0,1]
     */
    void setCuriosityThreshold(float threshold);

    /**
     * @brief Get curiosity threshold
     * @return Current curiosity threshold
     */
    float getCuriosityThreshold() const noexcept { return curiosity_threshold_.load(std::memory_order_relaxed); }

    /**
     * @brief Set uncertainty threshold for autonomous task generation
     * @param threshold Uncertainty threshold [0,1]
     */
    void setUncertaintyThreshold(float threshold);

    /**
     * @brief Get uncertainty threshold
     * @return Current uncertainty threshold
     */
    float getUncertaintyThreshold() const noexcept { return uncertainty_threshold_.load(std::memory_order_relaxed); }

    /**
     * @brief Set prediction error threshold
     * @param threshold Prediction error threshold [0,1]
     */
    void setPredictionErrorThreshold(float threshold);

    /**
     * @brief Get prediction error threshold
     * @return Current prediction error threshold
     */
    float getPredictionErrorThreshold() const noexcept { return prediction_error_threshold_.load(std::memory_order_relaxed); }

    /**
     * @brief Set maximum concurrent autonomous tasks
     * @param max_tasks Maximum number of concurrent tasks
     */
    void setMaxConcurrentTasks(int max_tasks);

    /**
     * @brief Get maximum concurrent tasks
     * @return Current maximum concurrent tasks
     */
    int getMaxConcurrentTasks() const noexcept { return max_concurrent_tasks_.load(std::memory_order_relaxed); }

    /**
     * @brief Set task generation interval
     * @param interval_ms Interval in milliseconds
     */
    void setTaskGenerationInterval(int interval_ms);

    /**
     * @brief Get task generation interval
     * @return Current task generation interval in ms
     */
    int getTaskGenerationInterval() const noexcept { return task_generation_interval_.load(std::memory_order_relaxed); }

    /**
     * @brief Enable or disable scaffold elimination
     * @param enabled True to eliminate external scaffolds
     */
    void setEliminateScaffolds(bool enabled);

    /**
     * @brief Check if scaffold elimination is enabled
     * @return True if scaffold elimination is enabled
     */
    bool isEliminateScaffoldsEnabled() const noexcept { return eliminate_scaffolds_.load(std::memory_order_relaxed); }

    /**
     * @brief Enable or disable autonomy metrics
     * @param enabled True to enable autonomy metrics
     */
    void setAutonomyMetrics(bool enabled);

    /**
     * @brief Check if autonomy metrics are enabled
     * @return True if autonomy metrics are enabled
     */
    bool isAutonomyMetricsEnabled() const noexcept { return autonomy_metrics_enabled_.load(std::memory_order_relaxed); }

    /**
     * @brief Set autonomy target level
     * @param target Target autonomy level [0,1]
     */
    void setAutonomyTarget(float target);

    /**
     * @brief Get autonomy target level
     * @return Current autonomy target level
     */
    float getAutonomyTarget() const noexcept { return autonomy_target_.load(std::memory_order_relaxed); }

    /**
     * @brief Set motivation decay rate
     * @param decay Motivation decay rate [0,1]
     */
    void setMotivationDecay(float decay);

    /**
     * @brief Get motivation decay rate
     * @return Current motivation decay rate
     */
    float getMotivationDecay() const noexcept { return motivation_decay_.load(std::memory_order_relaxed); }

    /**
     * @brief Set exploration bonus
     * @param bonus Exploration bonus value
     */
    void setExplorationBonus(float bonus);

    /**
     * @brief Get exploration bonus
     * @return Current exploration bonus
     */
    float getExplorationBonus() const noexcept { return exploration_bonus_.load(std::memory_order_relaxed); }

    /**
     * @brief Set novelty memory size
     * @param size Novelty memory size
     */
    void setNoveltyMemorySize(int size);

    /**
     * @brief Get novelty memory size
     * @return Current novelty memory size
     */
    int getNoveltyMemorySize() const noexcept { return novelty_memory_size_.load(std::memory_order_relaxed); }

private:
    /**
     * @brief Update global statistics
     */
    void updateGlobalStatistics();

    /**
     * @brief Update hardware information
     */
    void updateHardwareInfo();

    /**
     * @brief Process regions in sequential mode
     * @param delta_time Time step
     */
    void processSequential(float delta_time);

    /**
     * @brief Process regions in parallel mode
     * @param delta_time Time step
     */
    void processParallel(float delta_time);

    /**
     * @brief Process regions in hierarchical mode
     * @param delta_time Time step
     */
    void processHierarchical(float delta_time);

    /**
     * @brief Process regions in custom order
     * @param delta_time Time step
     */
    void processCustomOrder(float delta_time);

    /**
     * @brief Execute pre-processing callbacks
     * @param delta_time Time step
     */
    void executePreProcessingCallbacks(float delta_time);

    /**
     * @brief Execute post-processing callbacks
     * @param delta_time Time step
     */
    void executePostProcessingCallbacks(float delta_time);

    /**
     * @brief Update frequency calculation
     * @param delta_time Time step
     */
    void updateFrequencyCalculation(float delta_time);

    // New: Phase 3 - Experience storage structures
    struct ExperienceRecord {
        std::uint64_t timestamp_ms{0};
        std::uint64_t step{0};
        std::string tag;
        std::vector<float> input;
        std::vector<float> output;
        bool significant{false};
    };

    struct EpisodeRecord {
        std::string name;
        std::uint64_t start_ms{0};
        std::uint64_t end_ms{0};
        std::vector<std::size_t> experience_indices; // indices into experience_buffer_
    };

    // Checkpoint format versioning
    static constexpr int kCheckpointFormatVersion = 1;

    // Experience buffer (ring semantics when exceeding capacity)
    std::vector<ExperienceRecord> experience_buffer_;
    std::vector<EpisodeRecord> episodes_;
    std::size_t experience_capacity_{10000};

    // MemoryDB logging
    std::shared_ptr<NeuroForge::Core::MemoryDB> memory_db_;
    std::int64_t memory_db_run_id_{0};
    std::int64_t current_episode_id_{-1};
    bool memdb_propagation_debug_{false};
    bool memdb_colorize_{false};

    std::function<void(NeuroForge::NeuronID, NeuroForge::TimePoint)> spike_observer_;
};

} // namespace Core
} // namespace NeuroForge
